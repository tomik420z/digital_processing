#include "signal_processor.h"
#include <algorithm>
#include <numeric>
#include <cmath>

std::pair<SignalProcessor::Signal, long long> SignalProcessor::measurePerformance(const Signal& input) {
    auto start = std::chrono::high_resolution_clock::now();

    Signal result = process(input);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    return std::make_pair(result, duration.count());
}

double SignalProcessor::median(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    size_t size = values.size();

    if (size % 2 == 0) {
        return (values[size/2 - 1] + values[size/2]) / 2.0;
    } else {
        return values[size/2];
    }
}

double SignalProcessor::mad(const std::vector<double>& values, double med) {
    std::vector<double> deviations;
    deviations.reserve(values.size());

    for (double value : values) {
        deviations.push_back(std::abs(value - med));
    }

    return median(deviations);
}

double SignalProcessor::linearInterpolate(double x1, double y1, double x2, double y2, double x) {
    if (std::abs(x2 - x1) < 1e-10) {
        return y1;
    }
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

double calculateSNR(const SignalProcessor::Signal& clean, const SignalProcessor::Signal& noisy) {
    if (clean.size() != noisy.size() || clean.empty()) {
        return 0.0;
    }

    double signal_power = 0.0;
    double noise_power = 0.0;

    for (size_t i = 0; i < clean.size(); ++i) {
        signal_power += clean[i] * clean[i];
        double noise = noisy[i] - clean[i];
        noise_power += noise * noise;
    }

    signal_power /= clean.size();
    noise_power /= clean.size();

    if (noise_power < 1e-10) {
        return 100.0; // Очень высокое SNR для практически отсутствующего шума
    }

    return 10.0 * std::log10(signal_power / noise_power);
}

double calculateMSE(const SignalProcessor::Signal& original, const SignalProcessor::Signal& processed) {
    if (original.size() != processed.size() || original.empty()) {
        return 0.0;
    }

    double mse = 0.0;
    for (size_t i = 0; i < original.size(); ++i) {
        double diff = original[i] - processed[i];
        mse += diff * diff;
    }

    return mse / original.size();
}

double calculateCorrelation(const SignalProcessor::Signal& signal1, const SignalProcessor::Signal& signal2) {
    if (signal1.size() != signal2.size() || signal1.empty()) {
        return 0.0;
    }

    // Вычисляем средние значения
    double mean1 = std::accumulate(signal1.begin(), signal1.end(), 0.0) / signal1.size();
    double mean2 = std::accumulate(signal2.begin(), signal2.end(), 0.0) / signal2.size();

    // Вычисляем числитель и знаменатель корреляции
    double numerator = 0.0;
    double sum_sq1 = 0.0;
    double sum_sq2 = 0.0;

    for (size_t i = 0; i < signal1.size(); ++i) {
        double diff1 = signal1[i] - mean1;
        double diff2 = signal2[i] - mean2;

        numerator += diff1 * diff2;
        sum_sq1 += diff1 * diff1;
        sum_sq2 += diff2 * diff2;
    }

    double denominator = std::sqrt(sum_sq1 * sum_sq2);

    if (denominator < 1e-10) {
        return 0.0;
    }

    return numerator / denominator;
}