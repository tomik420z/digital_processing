#include "signal_classifier.h"
#include "utils/median.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Конструктор
// ─────────────────────────────────────────────────────────────────────────────

SignalClassifier::SignalClassifier(size_t localWindow, double sparseEps)
    : localWindow_(localWindow), sparseEps_(sparseEps)
{
    if (localWindow_ == 0) localWindow_ = 1;
    if (localWindow_ % 2 == 0) ++localWindow_; // нечётное
}

// ─────────────────────────────────────────────────────────────────────────────
// Вспомогательные статические методы
// ─────────────────────────────────────────────────────────────────────────────

double SignalClassifier::mean(const Signal& x)
{
    if (x.empty()) return 0.0;
    return std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
}

double SignalClassifier::variance(const Signal& x, double m)
{
    if (x.size() < 2) return 0.0;
    double v = 0.0;
    for (double val : x) v += (val - m) * (val - m);
    return v / static_cast<double>(x.size());
}

double SignalClassifier::rms(const Signal& x)
{
    if (x.empty()) return 0.0;
    double s = 0.0;
    for (double val : x) s += val * val;
    return std::sqrt(s / static_cast<double>(x.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Извлечение признаков
// ─────────────────────────────────────────────────────────────────────────────

SignalClassifier::Features SignalClassifier::extractFeatures(const Signal& signal) const
{
    Features f;
    const size_t N = signal.size();
    if (N < 4) return f;

    // ── Базовая статистика ────────────────────────────────────────────────────
    const double m   = mean(signal);
    const double var = variance(signal, m);
    const double sd  = (var > 0.0) ? std::sqrt(var) : 1e-12;
    const double r   = rms(signal);
    const double eps = (r > 0.0) ? r : 1e-12;

    // ── Куртозис ─────────────────────────────────────────────────────────────
    // k = (1/N * Σ(x-m)^4) / σ^4 — для гауссова шума ≈ 3, для импульсов >> 3
    {
        double k4 = 0.0;
        for (double v : signal) {
            double d = (v - m) / sd;
            k4 += d * d * d * d;
        }
        f.kurtosis = k4 / static_cast<double>(N);
    }

    // ── Пик-фактор (Crest Factor) ─────────────────────────────────────────────
    // CF = max|x| / RMS — для синуса ≈√2≈1.41, для меандра = 1, для импульсов >> 1
    {
        double maxAbs = 0.0;
        for (double v : signal) maxAbs = std::max(maxAbs, std::abs(v));
        f.crestFactor = maxAbs / eps;
    }

    // ── Частота пересечений нуля (Zero-Crossing Rate) ─────────────────────────
    // ZCR = число смен знака / (N-1)
    // Для синуса с нормированной частотой 0.1 → ~0.2 (≈ 2*freq)
    // Для меандра — аналогично, но с резкими переходами
    // Для эхо-сигнала — очень маленькое (сигнал большей частью = 0)
    {
        size_t crossings = 0;
        for (size_t i = 1; i < N; ++i) {
            if ((signal[i] >= 0.0) != (signal[i-1] >= 0.0))
                ++crossings;
        }
        f.zeroCrossingRate = static_cast<double>(crossings) / static_cast<double>(N - 1);
    }

    // ── Разреженность (Sparsity) ──────────────────────────────────────────────
    // Доля отсчётов с |x[n]| < sparseEps * max|x|
    // Для эхо-сигналов >> 0.8, для непрерывных сигналов ≈ 0
    {
        double maxAbs = 0.0;
        for (double v : signal) maxAbs = std::max(maxAbs, std::abs(v));
        const double thresh = sparseEps_ * maxAbs;
        size_t sparse = 0;
        for (double v : signal)
            if (std::abs(v) < thresh) ++sparse;
        f.sparsity = static_cast<double>(sparse) / static_cast<double>(N);
    }

    // ── Гладкость (Smoothness) ────────────────────────────────────────────────
    // Среднее абсолютное первое различие, нормированное на RMS
    // Синус → малое, меандр → среднее (редкие большие скачки), треугольник → малое-среднее
    {
        double diffSum = 0.0;
        for (size_t i = 1; i < N; ++i)
            diffSum += std::abs(signal[i] - signal[i-1]);
        f.smoothness = (diffSum / static_cast<double>(N - 1)) / eps;
    }

    // ── Отношение локальных дисперсий (LocalVarRatio) ─────────────────────────
    // Сравниваем максимальную и среднюю локальную дисперсию в окне localWindow_.
    // Для стационарного синуса → близко к 1.
    // Для импульсного сигнала с редкими пиками → сильно > 1.
    {
        const size_t half = localWindow_ / 2;
        double maxLV = 0.0, sumLV = 0.0;
        size_t cnt = 0;

        for (size_t n = half; n + half < N; n += half) {
            const size_t lo = n - half;
            const size_t hi = std::min(n + half, N - 1);
            double lm = 0.0;
            const size_t len = hi - lo + 1;
            for (size_t k = lo; k <= hi; ++k) lm += signal[k];
            lm /= static_cast<double>(len);
            double lv = 0.0;
            for (size_t k = lo; k <= hi; ++k)
                lv += (signal[k] - lm) * (signal[k] - lm);
            lv /= static_cast<double>(len);
            maxLV  = std::max(maxLV, lv);
            sumLV += lv;
            ++cnt;
        }
        const double meanLV = (cnt > 0 && sumLV > 0.0)
                              ? (sumLV / static_cast<double>(cnt)) : 1e-12;
        f.localVarRatio = maxLV / meanLV;
    }

    // ── Range Ratio ───────────────────────────────────────────────────────────
    // (max - min) / 2 / RMS
    // Для синуса ≈ 1.0 (амплитуда ≈ RMS * √2, half-range = амплитуда)
    // Для меандра = 1.0 (RMS = амплитуда, range/2 = амплитуда)
    // Для сигнала с огромными выбросами >> 1
    {
        double maxV = *std::max_element(signal.begin(), signal.end());
        double minV = *std::min_element(signal.begin(), signal.end());
        f.rangeRatio = (maxV - minV) / 2.0 / eps;
    }

    return f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Классификация по признакам
// Правила составлены эмпирически по характеристикам сигналов проекта.
// ─────────────────────────────────────────────────────────────────────────────

SignalClassifier::SignalType
SignalClassifier::classify(const Features& f) const
{
    // ── Импульсные помехи (NOISY) ─────────────────────────────────────────────
    // Признаки: очень высокий куртозис И высокий rangeRatio
    // (несинхронные выбросы дают острые «хвосты» распределения)
    if (f.kurtosis > 10.0 && f.rangeRatio > 3.0) {
        return SignalType::NOISY;
    }

    // ── Эхо-сигнал (ECHO) ────────────────────────────────────────────────────
    // Признаки: высокая разреженность + высокий пик-фактор
    // (большинство отсчётов ≈ 0, лишь несколько пиков)
    if (f.sparsity > 0.65 && f.crestFactor > 3.0) {
        return SignalType::ECHO;
    }

    // ── Меандр / квадратный сигнал (SQUARE) ───────────────────────────────────
    // Признаки: низкий куртозис (бимодальное, не остроконечное) + высокая гладкость
    // (мало скачков, но они большие) + RangeRatio близко к 1
    // Отличие от синуса: у синуса smoothness мало, у меандра — умеренное при
    // малом ZCR. Дополнительный признак: kuртозис < 2 (плоское распределение)
    if (f.kurtosis < 2.5 && f.smoothness > 0.05 && f.rangeRatio < 2.0) {
        return SignalType::SQUARE;
    }

    // ── Треугольный / пилообразный сигнал (TRIANGLE) ─────────────────────────
    // Признаки: куртозис ≈ 1.8 (равномерное распределение значений)
    //           умеренная гладкость (монотонные линейные участки)
    //           ZCR умеренная
    if (f.kurtosis < 3.0 && f.smoothness > 0.01 && f.smoothness < 0.15
        && f.zeroCrossingRate > 0.05) {
        return SignalType::TRIANGLE;
    }

    // ── Синусоидальный сигнал (SINE) ──────────────────────────────────────────
    // Признаки: куртозис ≈ 1.5 (синус имеет характерное значение ~1.5)
    //           низкая гладкость (медленные изменения)
    //           умеренный ZCR
    if (f.kurtosis < 4.0 && f.smoothness < 0.12 && f.zeroCrossingRate > 0.03) {
        return SignalType::SINE;
    }

    return SignalType::UNKNOWN;
}

// ─────────────────────────────────────────────────────────────────────────────
// Объединённый метод
// ─────────────────────────────────────────────────────────────────────────────

SignalClassifier::SignalType
SignalClassifier::classifySignal(const Signal& signal) const
{
    return classify(extractFeatures(signal));
}

// ─────────────────────────────────────────────────────────────────────────────
// Строковое представление
// ─────────────────────────────────────────────────────────────────────────────

std::string SignalClassifier::typeToString(SignalType type)
{
    switch (type) {
        case SignalType::SINE:     return "SINE";
        case SignalType::SQUARE:   return "SQUARE";
        case SignalType::TRIANGLE: return "TRIANGLE";
        case SignalType::ECHO:     return "ECHO";
        case SignalType::NOISY:    return "NOISY";
        default:                   return "UNKNOWN";
    }
}
