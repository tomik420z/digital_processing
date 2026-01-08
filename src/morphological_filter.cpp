#include "morphological_filter.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

MorphologicalFilter::MorphologicalFilter(Operation operation, size_t elementSize)
    : operation_(operation), structuringElement_(createFlatElement(elementSize)) {
}

MorphologicalFilter::MorphologicalFilter(Operation operation, const std::vector<double>& structuringElement)
    : operation_(operation), structuringElement_(structuringElement) {

    if (structuringElement_.empty()) {
        throw std::invalid_argument("Structuring element cannot be empty");
    }
}

SignalProcessor::Signal MorphologicalFilter::process(const Signal& input) {
    if (input.empty()) {
        return Signal();
    }

    switch (operation_) {
        case Operation::EROSION:
            return erosion(input);
        case Operation::DILATION:
            return dilation(input);
        case Operation::OPENING:
            return opening(input);
        case Operation::CLOSING:
            return closing(input);
        default:
            return input; // Не должно произойти
    }
}

std::string MorphologicalFilter::getName() const {
    return "MorphologicalFilter_" + operationToString(operation_) + "_" +
           std::to_string(structuringElement_.size());
}

void MorphologicalFilter::setOperation(Operation operation) {
    operation_ = operation;
}

void MorphologicalFilter::setStructuringElement(const std::vector<double>& structuringElement) {
    if (structuringElement.empty()) {
        throw std::invalid_argument("Structuring element cannot be empty");
    }
    structuringElement_ = structuringElement;
}

SignalProcessor::Signal MorphologicalFilter::erosion(const Signal& input) const {
    Signal result;
    result.reserve(input.size());

    size_t halfSize = structuringElement_.size() / 2;

    for (size_t i = 0; i < input.size(); ++i) {
        double minVal = std::numeric_limits<double>::max();

        for (size_t j = 0; j < structuringElement_.size(); ++j) {
            // Вычисляем индекс в исходном сигнале
            int signalIndex = static_cast<int>(i) - static_cast<int>(halfSize) + static_cast<int>(j);

            if (signalIndex >= 0 && signalIndex < static_cast<int>(input.size())) {
                double value = input[signalIndex] - structuringElement_[j];
                minVal = std::min(minVal, value);
            }
        }

        if (minVal == std::numeric_limits<double>::max()) {
            minVal = input[i]; // Если ничего не найдено, берем исходное значение
        }

        result.push_back(minVal);
    }

    return result;
}

SignalProcessor::Signal MorphologicalFilter::dilation(const Signal& input) const {
    Signal result;
    result.reserve(input.size());

    size_t halfSize = structuringElement_.size() / 2;

    for (size_t i = 0; i < input.size(); ++i) {
        double maxVal = std::numeric_limits<double>::lowest();

        for (size_t j = 0; j < structuringElement_.size(); ++j) {
            // Вычисляем индекс в исходном сигнале
            int signalIndex = static_cast<int>(i) - static_cast<int>(halfSize) + static_cast<int>(j);

            if (signalIndex >= 0 && signalIndex < static_cast<int>(input.size())) {
                double value = input[signalIndex] + structuringElement_[j];
                maxVal = std::max(maxVal, value);
            }
        }

        if (maxVal == std::numeric_limits<double>::lowest()) {
            maxVal = input[i]; // Если ничего не найдено, берем исходное значение
        }

        result.push_back(maxVal);
    }

    return result;
}

SignalProcessor::Signal MorphologicalFilter::opening(const Signal& input) const {
    // Размыкание = эрозия + дилатация
    Signal eroded = erosion(input);

    // Создаем временный фильтр для дилатации
    MorphologicalFilter dilationFilter(Operation::DILATION, structuringElement_);
    return dilationFilter.dilation(eroded);
}

SignalProcessor::Signal MorphologicalFilter::closing(const Signal& input) const {
    // Замыкание = дилатация + эрозия
    Signal dilated = dilation(input);

    // Создаем временный фильтр для эрозии
    MorphologicalFilter erosionFilter(Operation::EROSION, structuringElement_);
    return erosionFilter.erosion(dilated);
}

std::vector<double> MorphologicalFilter::createFlatElement(size_t size) {
    if (size == 0) {
        throw std::invalid_argument("Element size must be positive");
    }

    // Создаем плоский структурирующий элемент (все значения равны нулю)
    return std::vector<double>(size, 0.0);
}

std::string MorphologicalFilter::operationToString(Operation operation) {
    switch (operation) {
        case Operation::EROSION:
            return "Erosion";
        case Operation::DILATION:
            return "Dilation";
        case Operation::OPENING:
            return "Opening";
        case Operation::CLOSING:
            return "Closing";
        default:
            return "Unknown";
    }
}