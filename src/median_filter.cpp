#include "median_filter.h"
#include <algorithm>
#include <stdexcept>

MedianFilter::MedianFilter(size_t windowSize) {
    setWindowSize(windowSize);
}

SignalProcessor::Signal MedianFilter::process(const Signal& input) {
    if (input.empty()) {
        return Signal();
    }

    Signal output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        output.push_back(computeWindowMedian(input, i));
    }

    return output;
}

std::string MedianFilter::getName() const {
    return "MedianFilter_" + std::to_string(windowSize_);
}

void MedianFilter::setWindowSize(size_t windowSize) {
    if (!IsValidWindowSize(windowSize)) {
        throw std::invalid_argument("Window size must be positive and odd");
    }
    windowSize_ = windowSize;
}

size_t MedianFilter::getWindowSize() const {
    return windowSize_;
}

double MedianFilter::computeWindowMedian(const Signal& input, size_t index) const {
    std::vector<double> window;
    size_t halfWindow = windowSize_ / 2;

    // Определяем границы окна с учетом краев сигнала
    size_t startIdx = (index >= halfWindow) ? index - halfWindow : 0;
    size_t endIdx = std::min(index + halfWindow + 1, input.size());

    // Заполняем окно
    for (size_t i = startIdx; i < endIdx; ++i) {
        window.push_back(input[i]);
    }

    // Обработка краев сигнала: дополняем окно копированием крайних значений
    if (index < halfWindow) {
        // Дополняем начало
        size_t padding = halfWindow - index;
        for (size_t i = 0; i < padding; ++i) {
            window.insert(window.begin(), input[0]);
        }
    }

    if (index + halfWindow >= input.size()) {
        // Дополняем конец
        size_t padding = index + halfWindow + 1 - input.size();
        for (size_t i = 0; i < padding; ++i) {
            window.push_back(input.back());
        }
    }

    return median(window);
}

bool MedianFilter::IsValidWindowSize(size_t windowSize) const {
    return windowSize != 0 && windowSize % 2 != 0;
}
