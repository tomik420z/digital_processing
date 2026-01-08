#include "outlier_detection.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>

OutlierDetection::OutlierDetection(DetectionMethod detectionMethod,
                                   InterpolationMethod interpolationMethod,
                                   double threshold,
                                   size_t windowSize)
    : detectionMethod_(detectionMethod),
      interpolationMethod_(interpolationMethod),
      threshold_(threshold),
      windowSize_(windowSize),
      arOrder_(5) {

    if (threshold <= 0.0) {
        throw std::invalid_argument("Threshold must be positive");
    }
    if (windowSize == 0 || windowSize % 2 == 0) {
        throw std::invalid_argument("Window size must be positive and odd");
    }
}

SignalProcessor::Signal OutlierDetection::process(const Signal& input) {
    if (input.empty()) {
        return Signal();
    }

    // Обнаруживаем выбросы
    std::vector<bool> outliers = detectOutliers(input);

    // Применяем интерполяцию для замещения выбросов
    switch (interpolationMethod_) {
        case InterpolationMethod::LINEAR:
            return interpolateLinear(input, outliers);
        case InterpolationMethod::MEDIAN_BASED:
            return interpolateMedian(input, outliers);
        case InterpolationMethod::AUTOREGRESSIVE:
            return interpolateAutoregressive(input, outliers);
        case InterpolationMethod::SPLINE:
            // Упрощенная версия - используем линейную интерполяцию
            return interpolateLinear(input, outliers);
        default:
            return input;
    }
}

std::string OutlierDetection::getName() const {
    return "OutlierDetection_" + detectionMethodToString(detectionMethod_) + "_" +
           interpolationMethodToString(interpolationMethod_) + "_" +
           std::to_string(static_cast<int>(threshold_ * 100)) + "_" +
           std::to_string(windowSize_);
}

void OutlierDetection::setParameters(DetectionMethod detectionMethod,
                                     InterpolationMethod interpolationMethod,
                                     double threshold,
                                     size_t windowSize) {
    if (threshold <= 0.0) {
        throw std::invalid_argument("Threshold must be positive");
    }
    if (windowSize == 0 || windowSize % 2 == 0) {
        throw std::invalid_argument("Window size must be positive and odd");
    }

    detectionMethod_ = detectionMethod;
    interpolationMethod_ = interpolationMethod;
    threshold_ = threshold;
    windowSize_ = windowSize;
}

std::vector<bool> OutlierDetection::detectOutliers(const Signal& input) const {
    switch (detectionMethod_) {
        case DetectionMethod::MAD_BASED:
            return detectMADBased(input);
        case DetectionMethod::STATISTICAL:
            return detectStatistical(input);
        case DetectionMethod::ADAPTIVE_THRESHOLD:
            return detectAdaptiveThreshold(input);
        default:
            return std::vector<bool>(input.size(), false);
    }
}

std::vector<bool> OutlierDetection::detectMADBased(const Signal& input) const {
    std::vector<bool> outliers(input.size(), false);

    size_t halfWindow = windowSize_ / 2;

    for (size_t i = 0; i < input.size(); ++i) {
        // Определяем окно вокруг текущей точки
        size_t startIdx = (i >= halfWindow) ? i - halfWindow : 0;
        size_t endIdx = std::min(i + halfWindow + 1, input.size());

        // Извлекаем значения в окне
        std::vector<double> window;
        for (size_t j = startIdx; j < endIdx; ++j) {
            window.push_back(input[j]);
        }

        if (window.size() < 3) {
            continue; // Недостаточно данных для анализа
        }

        // Вычисляем медиану и MAD
        double med = median(window);
        double madValue = mad(window, med);

        // Проверяем, является ли текущее значение выбросом
        if (madValue > 0.0) {
            double deviation = std::abs(input[i] - med);
            if (deviation > threshold_ * madValue) {
                outliers[i] = true;
            }
        }
    }

    return outliers;
}

std::vector<bool> OutlierDetection::detectStatistical(const Signal& input) const {
    std::vector<bool> outliers(input.size(), false);

    // Вычисляем среднее и стандартное отклонение
    double mean = std::accumulate(input.begin(), input.end(), 0.0) / input.size();

    double variance = 0.0;
    for (double value : input) {
        variance += (value - mean) * (value - mean);
    }
    variance /= input.size();
    double stddev = std::sqrt(variance);

    if (stddev == 0.0) {
        return outliers; // Нет вариации в данных
    }

    // Проверяем каждую точку
    for (size_t i = 0; i < input.size(); ++i) {
        double zscore = std::abs(input[i] - mean) / stddev;
        if (zscore > threshold_) {
            outliers[i] = true;
        }
    }

    return outliers;
}

std::vector<bool> OutlierDetection::detectAdaptiveThreshold(const Signal& input) const {
    std::vector<bool> outliers(input.size(), false);

    size_t halfWindow = windowSize_ / 2;

    for (size_t i = 0; i < input.size(); ++i) {
        // Определяем адаптивное окно
        size_t startIdx = (i >= halfWindow) ? i - halfWindow : 0;
        size_t endIdx = std::min(i + halfWindow + 1, input.size());

        // Вычисляем локальные статистики
        double localSum = 0.0;
        size_t count = 0;

        for (size_t j = startIdx; j < endIdx; ++j) {
            if (j != i) { // Исключаем текущую точку
                localSum += input[j];
                count++;
            }
        }

        if (count == 0) {
            continue;
        }

        double localMean = localSum / count;

        // Вычисляем локальное стандартное отклонение
        double localVariance = 0.0;
        for (size_t j = startIdx; j < endIdx; ++j) {
            if (j != i) {
                double diff = input[j] - localMean;
                localVariance += diff * diff;
            }
        }
        localVariance /= count;
        double localStddev = std::sqrt(localVariance);

        // Адаптивный порог
        double adaptiveThreshold = threshold_ * localStddev;
        if (localStddev == 0.0) {
            adaptiveThreshold = threshold_;
        }

        // Проверяем текущую точку
        if (std::abs(input[i] - localMean) > adaptiveThreshold) {
            outliers[i] = true;
        }
    }

    return outliers;
}

SignalProcessor::Signal OutlierDetection::interpolateLinear(const Signal& input,
                                                            const std::vector<bool>& outliers) const {
    Signal result = input;

    for (size_t i = 0; i < outliers.size(); ++i) {
        if (outliers[i]) {
            auto [leftIdx, rightIdx] = findNearestNormalPoints(outliers, i);

            if (leftIdx >= 0 && rightIdx >= 0) {
                // Линейная интерполяция между двумя нормальными точками
                result[i] = linearInterpolate(leftIdx, input[leftIdx],
                                            rightIdx, input[rightIdx],
                                            static_cast<double>(i));
            } else if (leftIdx >= 0) {
                // Только левая точка доступна
                result[i] = input[leftIdx];
            } else if (rightIdx >= 0) {
                // Только правая точка доступна
                result[i] = input[rightIdx];
            }
            // Если обе точки недоступны, оставляем исходное значение
        }
    }

    return result;
}

SignalProcessor::Signal OutlierDetection::interpolateMedian(const Signal& input,
                                                            const std::vector<bool>& outliers) const {
    Signal result = input;
    size_t halfWindow = std::min(windowSize_ / 2, static_cast<size_t>(5));

    for (size_t i = 0; i < outliers.size(); ++i) {
        if (outliers[i]) {
            std::vector<double> neighbors;

            // Собираем нормальные соседние точки
            for (size_t j = (i >= halfWindow ? i - halfWindow : 0);
                 j < std::min(i + halfWindow + 1, input.size()); ++j) {
                if (j != i && !outliers[j]) {
                    neighbors.push_back(input[j]);
                }
            }

            if (!neighbors.empty()) {
                result[i] = median(neighbors);
            }
        }
    }

    return result;
}

SignalProcessor::Signal OutlierDetection::interpolateAutoregressive(const Signal& input,
                                                                    const std::vector<bool>& outliers) const {
    Signal result = input;

    // Упрощенная AR модель: используем взвешенное среднее предыдущих значений
    for (size_t i = 0; i < outliers.size(); ++i) {
        if (outliers[i]) {
            double sum = 0.0;
            double weightSum = 0.0;
            size_t usedPoints = 0;

            // Используем предыдущие нормальные точки
            for (size_t j = 1; j <= arOrder_ && j <= i; ++j) {
                size_t idx = i - j;
                if (!outliers[idx]) {
                    double weight = 1.0 / j; // Обратно пропорциональный вес
                    sum += weight * result[idx];
                    weightSum += weight;
                    usedPoints++;
                }
            }

            if (weightSum > 0.0) {
                result[i] = sum / weightSum;
            } else {
                // Fallback к линейной интерполяции
                auto interpolated = interpolateLinear(input, outliers);
                result[i] = interpolated[i];
            }
        }
    }

    return result;
}

std::pair<int, int> OutlierDetection::findNearestNormalPoints(const std::vector<bool>& outliers,
                                                              size_t index) const {
    int leftIdx = -1;
    int rightIdx = -1;

    // Поиск левой нормальной точки
    for (int i = static_cast<int>(index) - 1; i >= 0; --i) {
        if (!outliers[i]) {
            leftIdx = i;
            break;
        }
    }

    // Поиск правой нормальной точки
    for (size_t i = index + 1; i < outliers.size(); ++i) {
        if (!outliers[i]) {
            rightIdx = static_cast<int>(i);
            break;
        }
    }

    return std::make_pair(leftIdx, rightIdx);
}

std::string OutlierDetection::detectionMethodToString(DetectionMethod method) {
    switch (method) {
        case DetectionMethod::MAD_BASED:
            return "MAD";
        case DetectionMethod::STATISTICAL:
            return "Statistical";
        case DetectionMethod::ADAPTIVE_THRESHOLD:
            return "Adaptive";
        default:
            return "Unknown";
    }
}

std::string OutlierDetection::interpolationMethodToString(InterpolationMethod method) {
    switch (method) {
        case InterpolationMethod::LINEAR:
            return "Linear";
        case InterpolationMethod::SPLINE:
            return "Spline";
        case InterpolationMethod::MEDIAN_BASED:
            return "Median";
        case InterpolationMethod::AUTOREGRESSIVE:
            return "AR";
        default:
            return "Unknown";
    }
}