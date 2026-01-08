#include "savgol_filter.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>

SavgolFilter::SavgolFilter(size_t windowSize, size_t polyOrder)
    : windowSize_(windowSize), polyOrder_(polyOrder) {

    if (windowSize == 0 || windowSize % 2 == 0) {
        throw std::invalid_argument("Window size must be positive and odd");
    }
    if (polyOrder >= windowSize) {
        throw std::invalid_argument("Polynomial order must be less than window size");
    }

    calculateCoefficients();
}

SignalProcessor::Signal SavgolFilter::process(const Signal& input) {
    if (input.empty()) {
        return Signal();
    }

    Signal output;
    output.reserve(input.size());

    // Применяем фильтр к каждой точке
    for (size_t i = 0; i < input.size(); ++i) {
        output.push_back(applyFilter(input, i));
    }

    return output;
}

std::string SavgolFilter::getName() const {
    return "SavgolFilter_" + std::to_string(windowSize_) + "_" + std::to_string(polyOrder_);
}

void SavgolFilter::setParameters(size_t windowSize, size_t polyOrder) {
    if (windowSize == 0 || windowSize % 2 == 0) {
        throw std::invalid_argument("Window size must be positive and odd");
    }
    if (polyOrder >= windowSize) {
        throw std::invalid_argument("Polynomial order must be less than window size");
    }

    windowSize_ = windowSize;
    polyOrder_ = polyOrder;
    calculateCoefficients();
}

void SavgolFilter::calculateCoefficients() {
    // Создаем систему уравнений для метода наименьших квадратов
    size_t halfWindow = windowSize_ / 2;

    // Матрица Вандермонда для полиномиальной аппроксимации
    std::vector<std::vector<double>> matrix(polyOrder_ + 1, std::vector<double>(polyOrder_ + 1, 0.0));
    std::vector<double> rhs(polyOrder_ + 1, 0.0);

    // Заполняем матрицу коэффициентов
    for (size_t i = 0; i <= polyOrder_; ++i) {
        for (size_t j = 0; j <= polyOrder_; ++j) {
            double sum = 0.0;
            for (int k = -static_cast<int>(halfWindow); k <= static_cast<int>(halfWindow); ++k) {
                sum += std::pow(k, i + j);
            }
            matrix[i][j] = sum;
        }
    }

    // Правая часть уравнения (для центральной точки, k=0, только первая степень даёт 1)
    rhs[0] = 1.0; // Коэффициент при x^0 для точки x=0
    for (size_t i = 1; i <= polyOrder_; ++i) {
        rhs[i] = 0.0; // Коэффициенты при старших степенях равны 0 для центральной точки
    }

    // Решаем систему уравнений
    std::vector<double> polyCoeffs = gaussElimination(matrix, rhs);

    // Вычисляем коэффициенты фильтра
    coefficients_.clear();
    coefficients_.resize(windowSize_);

    for (size_t i = 0; i < windowSize_; ++i) {
        int k = static_cast<int>(i) - static_cast<int>(halfWindow);
        double coeff = 0.0;

        for (size_t j = 0; j <= polyOrder_; ++j) {
            coeff += polyCoeffs[j] * std::pow(k, j);
        }

        coefficients_[i] = coeff;
    }
}

std::vector<double> SavgolFilter::gaussElimination(std::vector<std::vector<double>>& matrix,
                                                   std::vector<double>& rhs) const {
    size_t n = matrix.size();

    // Прямой ход метода Гаусса
    for (size_t i = 0; i < n; ++i) {
        // Поиск главного элемента
        size_t maxRow = i;
        for (size_t k = i + 1; k < n; ++k) {
            if (std::abs(matrix[k][i]) > std::abs(matrix[maxRow][i])) {
                maxRow = k;
            }
        }

        // Перестановка строк
        if (maxRow != i) {
            std::swap(matrix[i], matrix[maxRow]);
            std::swap(rhs[i], rhs[maxRow]);
        }

        // Проверка на вырожденность
        if (std::abs(matrix[i][i]) < 1e-12) {
            throw std::runtime_error("Matrix is singular");
        }

        // Исключение
        for (size_t k = i + 1; k < n; ++k) {
            double factor = matrix[k][i] / matrix[i][i];
            for (size_t j = i; j < n; ++j) {
                matrix[k][j] -= factor * matrix[i][j];
            }
            rhs[k] -= factor * rhs[i];
        }
    }

    // Обратный ход
    std::vector<double> solution(n);
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        solution[i] = rhs[i];
        for (size_t j = i + 1; j < n; ++j) {
            solution[i] -= matrix[i][j] * solution[j];
        }
        solution[i] /= matrix[i][i];
    }

    return solution;
}

double SavgolFilter::applyFilter(const Signal& input, size_t centerIndex) const {
    double result = 0.0;
    size_t halfWindow = windowSize_ / 2;

    for (size_t i = 0; i < windowSize_; ++i) {
        int signalIndex = static_cast<int>(centerIndex) - static_cast<int>(halfWindow) + static_cast<int>(i);
        double value = getReflectedValue(input, signalIndex);
        result += coefficients_[i] * value;
    }

    return result;
}

double SavgolFilter::getReflectedValue(const Signal& input, int index) const {
    if (index < 0) {
        // Отражение в начале сигнала
        return input[-index];
    } else if (index >= static_cast<int>(input.size())) {
        // Отражение в конце сигнала
        int reflectedIndex = 2 * static_cast<int>(input.size()) - 2 - index;
        if (reflectedIndex < 0) {
            return input[0]; // Если всё равно выходим за границы, берем первое значение
        }
        return input[reflectedIndex];
    } else {
        return input[index];
    }
}