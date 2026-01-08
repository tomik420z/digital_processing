#include "wiener_filter.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>

WienerFilter::WienerFilter(size_t filterOrder, double mu, double lambda)
    : filterOrder_(filterOrder), mu_(mu), lambda_(lambda) {

    if (filterOrder == 0) {
        throw std::invalid_argument("Filter order must be positive");
    }
    if (mu <= 0.0 || mu >= 1.0) {
        throw std::invalid_argument("Adaptation step mu must be in (0, 1)");
    }
    if (lambda <= 0.0 || lambda > 1.0) {
        throw std::invalid_argument("Forgetting factor lambda must be in (0, 1]");
    }

    reset();
}

SignalProcessor::Signal WienerFilter::process(const Signal& input) {
    if (input.empty()) {
        return Signal();
    }

    // Используем алгоритм LMS для адаптации
    return processLMS(input);
}

std::string WienerFilter::getName() const {
    return "WienerFilter_" + std::to_string(filterOrder_) + "_" +
           std::to_string(static_cast<int>(mu_ * 1000)) + "_" +
           std::to_string(static_cast<int>(lambda_ * 1000));
}

void WienerFilter::setParameters(size_t filterOrder, double mu, double lambda) {
    if (filterOrder == 0) {
        throw std::invalid_argument("Filter order must be positive");
    }
    if (mu <= 0.0 || mu >= 1.0) {
        throw std::invalid_argument("Adaptation step mu must be in (0, 1)");
    }
    if (lambda <= 0.0 || lambda > 1.0) {
        throw std::invalid_argument("Forgetting factor lambda must be in (0, 1]");
    }

    filterOrder_ = filterOrder;
    mu_ = mu;
    lambda_ = lambda;
    reset();
}

void WienerFilter::reset() {
    weights_.assign(filterOrder_, 0.0);
    // Инициализация малыми случайными значениями
    for (size_t i = 0; i < filterOrder_; ++i) {
        weights_[i] = 0.001 * (static_cast<double>(rand()) / RAND_MAX - 0.5);
    }
}

SignalProcessor::Signal WienerFilter::processLMS(const Signal& input) {
    Signal output;
    output.reserve(input.size());

    // Буфер для задержанных отсчетов
    std::vector<double> delayBuffer(filterOrder_, 0.0);

    for (size_t n = 0; n < input.size(); ++n) {
        // Сдвигаем буфер и добавляем новый отсчет
        for (size_t i = filterOrder_ - 1; i > 0; --i) {
            delayBuffer[i] = delayBuffer[i-1];
        }
        delayBuffer[0] = input[n];

        // Вычисляем выход фильтра
        double y = 0.0;
        for (size_t i = 0; i < filterOrder_; ++i) {
            y += weights_[i] * delayBuffer[i];
        }

        // Оценка желаемого сигнала (предполагаем, что помехи имеют высокочастотную природу)
        double desired = input[n];
        if (n > 0) {
            desired = 0.5 * (input[n-1] + (n < input.size()-1 ? input[n+1] : input[n]));
        }

        // Вычисляем ошибку
        double error = desired - y;

        // Обновляем веса (алгоритм LMS)
        for (size_t i = 0; i < filterOrder_; ++i) {
            weights_[i] += mu_ * error * delayBuffer[i];
        }

        output.push_back(y);
    }

    return output;
}

SignalProcessor::Signal WienerFilter::processRLS(const Signal& input) {
    Signal output;
    output.reserve(input.size());

    // Инициализация для RLS
    const double delta = 0.001; // Параметр регуляризации
    std::vector<std::vector<double>> P(filterOrder_, std::vector<double>(filterOrder_, 0.0));

    // Инициализация обратной корреляционной матрицы
    for (size_t i = 0; i < filterOrder_; ++i) {
        P[i][i] = 1.0 / delta;
    }

    std::vector<double> delayBuffer(filterOrder_, 0.0);

    for (size_t n = 0; n < input.size(); ++n) {
        // Обновляем буфер задержки
        for (size_t i = filterOrder_ - 1; i > 0; --i) {
            delayBuffer[i] = delayBuffer[i-1];
        }
        delayBuffer[0] = input[n];

        // Вычисляем выход фильтра
        double y = 0.0;
        for (size_t i = 0; i < filterOrder_; ++i) {
            y += weights_[i] * delayBuffer[i];
        }

        // Желаемый сигнал (упрощенная оценка)
        double desired = input[n];
        if (n > 2 && n < input.size() - 2) {
            desired = (input[n-2] + input[n-1] + input[n] + input[n+1] + input[n+2]) / 5.0;
        }

        double error = desired - y;

        // Вычисляем коэффициент усиления Калмана
        std::vector<double> k(filterOrder_, 0.0);
        double denominator = lambda_;

        for (size_t i = 0; i < filterOrder_; ++i) {
            for (size_t j = 0; j < filterOrder_; ++j) {
                denominator += delayBuffer[i] * P[i][j] * delayBuffer[j];
            }
        }

        for (size_t i = 0; i < filterOrder_; ++i) {
            for (size_t j = 0; j < filterOrder_; ++j) {
                k[i] += P[i][j] * delayBuffer[j];
            }
            k[i] /= denominator;
        }

        // Обновляем веса
        for (size_t i = 0; i < filterOrder_; ++i) {
            weights_[i] += k[i] * error;
        }

        // Обновляем обратную корреляционную матрицу
        std::vector<std::vector<double>> P_new = P;
        for (size_t i = 0; i < filterOrder_; ++i) {
            for (size_t j = 0; j < filterOrder_; ++j) {
                P_new[i][j] = (P[i][j] - k[i] * delayBuffer[j]) / lambda_;
            }
        }
        P = P_new;

        output.push_back(y);
    }

    return output;
}

std::vector<double> WienerFilter::getDelayVector(const Signal& input, size_t index) const {
    std::vector<double> delayVector(filterOrder_, 0.0);

    for (size_t i = 0; i < filterOrder_; ++i) {
        if (index >= i) {
            delayVector[i] = input[index - i];
        } else {
            delayVector[i] = 0.0; // Нулевое дополнение для начала сигнала
        }
    }

    return delayVector;
}