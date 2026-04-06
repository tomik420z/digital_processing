#include "doppler_nip_filter.h"

#include <cmath>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Конструктор
// ─────────────────────────────────────────────────────────────────────────────

DopplerNipFilter::DopplerNipFilter(double detectionThreshold,
                                   double minRelativeAmplitude,
                                   int    phaseAveragingWindow)
    : detectionThreshold_(detectionThreshold)
    , minRelativeAmplitude_(minRelativeAmplitude)
    , phaseAveragingWindow_(phaseAveragingWindow)
{
}

void DopplerNipFilter::setParameters(double detectionThreshold,
                                     double minRelativeAmplitude,
                                     int    phaseAveragingWindow)
{
    detectionThreshold_   = detectionThreshold;
    minRelativeAmplitude_ = minRelativeAmplitude;
    phaseAveragingWindow_ = phaseAveragingWindow;
}

std::string DopplerNipFilter::getName() const
{
    return std::string("DopplerNipFilter_thr") +
           std::to_string(static_cast<int>(detectionThreshold_ * 100));
}

// ─────────────────────────────────────────────────────────────────────────────
// Вычисление нормированного ДПФ: Y[k] = (1/N) · Σ x[n]·exp(-j2πkn/N)
// ─────────────────────────────────────────────────────────────────────────────

ComplexSignal DopplerNipFilter::computeDFT(const ComplexSignal& x)
{
    const size_t N = x.size();
    if (N == 0) return CVector();

    // Используем готовый FFT из fft.h (Кули-Тьюки, дополнение до степени двойки)
    CVector a = x;
    const size_t Nfft = fft_impl::nextPow2(N);
    a.resize(Nfft, Complex(0.0, 0.0));
    fft_impl::fft_inplace(a, false);   // прямое ДПФ (без нормировки 1/N из fft_inplace)

    // fft_inplace возвращает Σ x[n]·exp(-j2πkn/N) без деления на N
    // Нормируем вручную: Y[k] = FFT[k] / N  (именно на исходный N, не на Nfft!)
    const double invN = 1.0 / static_cast<double>(N);
    for (auto& c : a)
        c *= invN;

    // Возвращаем только первые N бинов (без паддинга)
    a.resize(N);
    return a;
}

// ─────────────────────────────────────────────────────────────────────────────
// Обратное ДПФ: x̂[n] = IDFT{Y}
// ─────────────────────────────────────────────────────────────────────────────

ComplexSignal DopplerNipFilter::computeIDFT(const ComplexSignal& Y)
{
    const size_t N = Y.size();
    if (N == 0) return CVector();

    // fft_inplace с inv=true вычисляет IDFT со встроенной нормировкой 1/N
    CVector a = Y;
    const size_t Nfft = fft_impl::nextPow2(N);
    a.resize(Nfft, Complex(0.0, 0.0));
    fft_impl::fft_inplace(a, true);    // обратное ДПФ

    // Нас интересует только результат при нормировке на N (паддинг создаёт лишний масштаб).
    // fft_inplace(inv) делит на Nfft, но нам нужно делить на N.
    // Исправляем масштаб: умножаем на Nfft/N.
    const double scale = static_cast<double>(Nfft) / static_cast<double>(N);
    for (auto& c : a)
        c *= scale;

    a.resize(N);
    return a;
}

// ─────────────────────────────────────────────────────────────────────────────
// Обнаружение НИП по коэффициенту вариации модулей спектра
// ─────────────────────────────────────────────────────────────────────────────

NipDetectionResult DopplerNipFilter::detectNip(const ComplexSignal& Y) const
{
    NipDetectionResult result;
    const size_t N = Y.size();
    if (N < 2) return result;

    // ── Вычислить |Y[k]| ──────────────────────────────────────────────────
    std::vector<double> mags(N);
    for (size_t k = 0; k < N; ++k)
        mags[k] = std::abs(Y[k]);

    // ── Статистика модулей ─────────────────────────────────────────────────
    const double sum     = std::accumulate(mags.begin(), mags.end(), 0.0);
    const double mean    = sum / static_cast<double>(N);

    double sq_sum = 0.0;
    for (double m : mags)
        sq_sum += (m - mean) * (m - mean);
    const double stddev = std::sqrt(sq_sum / static_cast<double>(N));

    // Коэффициент вариации (CV): мера равномерности распределения |Y[k]|.
    // Малый CV → все бины имеют примерно одинаковый модуль → признак НИП.
    const double cv = (mean > 1e-12) ? (stddev / mean) : 1e9;
    result.detectionMetric = cv;

    // ── Минимальный модуль (оценка уровня НИП в каждом бине) ─────────────
    const double minMag  = *std::min_element(mags.begin(), mags.end());
    const double maxMag  = *std::max_element(mags.begin(), mags.end());

    // Критерий 1: CV ниже порога (равномерность спектра)
    const bool cv_ok     = (cv < detectionThreshold_);

    // Критерий 2: минимальный бин значим (не просто шум)
    const bool amp_ok    = (maxMag > 1e-12) &&
                           (minMag / maxMag >= minRelativeAmplitude_);

    result.detected = cv_ok && amp_ok;

    if (!result.detected)
        return result;

    // ── Оценка амплитуды НИП: Â = N · min(|Y[k]|) ───────────────────────
    result.amplitude = static_cast<double>(N) * minMag;

    // ── Оценка индекса поражённого импульса m ─────────────────────────────
    result.pulseIndex = estimatePulseIndex(Y, static_cast<int>(N));

    // ── Начальная фаза НИП φ₀ = arg(Y[0]) — arg(exp(-j·2π·0·m/N)) = arg(Y[0])
    result.phaseRad = std::arg(Y[0]);

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Оценка индекса поражённого импульса m по линейной фазе спектра.
//
// Если x[m] = A·exp(jφ₀) — НИП в импульсе m, то:
//   Y[k] = (A/N)·exp(jφ₀)·exp(-j2πkm/N)
//
// Фазовый наклон: Δφ[k] = arg(Y[k+1]) - arg(Y[k]) = -2π·m/N
//
// Усредняем Δφ по всем бинам (или окну) и находим m̂.
// ─────────────────────────────────────────────────────────────────────────────

int DopplerNipFilter::estimatePulseIndex(const ComplexSignal& Y, int N) const
{
    if (N < 2) return 0;

    // Определяем окно усреднения
    const int wnd = (phaseAveragingWindow_ > 0 && phaseAveragingWindow_ < N)
                    ? phaseAveragingWindow_
                    : (N - 1);

    // Накапливаем фазовые разности через произведение Y[k+1]·conj(Y[k])
    // Этот метод устойчивее к переходам через π, чем простая разность arg().
    Complex phasorSum(0.0, 0.0);
    for (int k = 0; k < wnd; ++k) {
        // Y[k+1] · conj(Y[k]) — комплексный фазор разности
        phasorSum += Y[(k + 1) % N] * std::conj(Y[k]);
    }

    // Средняя фазовая разность Δφ = arg(phasorSum)
    const double meanDeltaPhi = std::arg(phasorSum);

    // m̂ = round( −N · Δφ / (2π) )
    double mEstimate = -static_cast<double>(N) * meanDeltaPhi / (2.0 * M_PI);

    // Приводим в диапазон [0, N-1]
    int m = static_cast<int>(std::round(mEstimate));
    m = ((m % N) + N) % N;

    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
// Компенсация НИП из доплеровского спектра.
//
// Ŷ[k] = Y[k] − (Â/N)·exp(jφ₀)·exp(-j2πk·m̂/N)
// ─────────────────────────────────────────────────────────────────────────────

void DopplerNipFilter::compensateNip(ComplexSignal& Y,
                                     const NipDetectionResult& det,
                                     int N)
{
    if (!det.detected || N <= 0) return;

    const double nipLevelPerBin = det.amplitude / static_cast<double>(N);
    const Complex phaseFactor   = std::polar(1.0, det.phaseRad);  // exp(jφ₀)

    for (int k = 0; k < N; ++k) {
        // Вклад НИП в k-й бин: (Â/N)·exp(jφ₀)·exp(-j2πk·m/N)
        const double phaseK = -2.0 * M_PI * static_cast<double>(k) *
                               static_cast<double>(det.pulseIndex) /
                               static_cast<double>(N);
        const Complex nipContrib = nipLevelPerBin * phaseFactor *
                                   Complex(std::cos(phaseK), std::sin(phaseK));
        Y[k] -= nipContrib;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Преобразование в дБ: 20·log10(|Y[k]| + eps)
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal DopplerNipFilter::toDecibels(const ComplexSignal& Y)
{
    const double eps = 1e-12;
    SignalProcessor::Signal db(Y.size());
    for (size_t k = 0; k < Y.size(); ++k)
        db[k] = 20.0 * std::log10(std::abs(Y[k]) + eps);
    return db;
}

// ─────────────────────────────────────────────────────────────────────────────
// Разность спектров
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal DopplerNipFilter::getSpectrumDiff() const
{
    const size_t N = spectrumBefore_.size();
    SignalProcessor::Signal diff(N);
    for (size_t k = 0; k < N; ++k)
        diff[k] = spectrumBefore_[k] - spectrumAfter_[k];
    return diff;
}

// ─────────────────────────────────────────────────────────────────────────────
// Основная функция обработки одного дискрета дальности
// ─────────────────────────────────────────────────────────────────────────────

ComplexSignal DopplerNipFilter::process(const ComplexSignal& burstSamples)
{
    const size_t N = burstSamples.size();
    if (N == 0) return CVector();

    // ── Шаг 1: Доплеровский банк фильтров (ДПФ) ──────────────────────────
    ComplexSignal Y = computeDFT(burstSamples);

    // Сохраняем спектр ДО компенсации
    spectrumBefore_ = toDecibels(Y);

    // ── Шаг 2: Обнаружение НИП ───────────────────────────────────────────
    lastDetection_ = detectNip(Y);

    if (lastDetection_.detected) {
        std::cout << "[DopplerNipFilter] НИП обнаружена:\n"
                  << "  Индекс импульса m = " << lastDetection_.pulseIndex << "\n"
                  << "  Амплитуда         = " << std::fixed << std::setprecision(4)
                  << lastDetection_.amplitude << "\n"
                  << "  Начальная фаза    = " << std::fixed << std::setprecision(4)
                  << lastDetection_.phaseRad << " рад\n"
                  << "  CV-метрика        = " << std::fixed << std::setprecision(4)
                  << lastDetection_.detectionMetric << "\n";
    } else {
        std::cout << "[DopplerNipFilter] НИП не обнаружена (CV = "
                  << std::fixed << std::setprecision(4)
                  << lastDetection_.detectionMetric << ")\n";
    }

    // ── Шаги 3-4: Компенсация НИП ────────────────────────────────────────
    ComplexSignal Ycomp = Y;
    compensateNip(Ycomp, lastDetection_, static_cast<int>(N));

    // Сохраняем спектр ПОСЛЕ компенсации
    spectrumAfter_ = toDecibels(Ycomp);

    // ── Шаг 5: Восстановление временного сигнала (ИДПФ) ──────────────────
    ComplexSignal xOut = computeIDFT(Ycomp);

    return xOut;
}

// ─────────────────────────────────────────────────────────────────────────────
// Перегрузка: раздельные I/Q каналы
// ─────────────────────────────────────────────────────────────────────────────

ComplexSignal DopplerNipFilter::process(const SignalProcessor::Signal& iChannel,
                                        const SignalProcessor::Signal& qChannel)
{
    const size_t N = iChannel.size();
    if (N == 0) return CVector();
    if (qChannel.size() != N)
        throw std::invalid_argument("DopplerNipFilter::process: I и Q каналы разной длины");

    ComplexSignal cs(N);
    for (size_t n = 0; n < N; ++n)
        cs[n] = Complex(iChannel[n], qChannel[n]);

    return process(cs);
}

// ─────────────────────────────────────────────────────────────────────────────
// Утилиты
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal DopplerNipFilter::getRealPart(const ComplexSignal& cs)
{
    SignalProcessor::Signal out(cs.size());
    for (size_t i = 0; i < cs.size(); ++i)
        out[i] = cs[i].real();
    return out;
}

SignalProcessor::Signal DopplerNipFilter::getImagPart(const ComplexSignal& cs)
{
    SignalProcessor::Signal out(cs.size());
    for (size_t i = 0; i < cs.size(); ++i)
        out[i] = cs[i].imag();
    return out;
}

SignalProcessor::Signal DopplerNipFilter::getMagnitude(const ComplexSignal& cs)
{
    SignalProcessor::Signal out(cs.size());
    for (size_t i = 0; i < cs.size(); ++i)
        out[i] = std::abs(cs[i]);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Загрузка/сохранение CSV (формат Re,Im на строку)
// ─────────────────────────────────────────────────────────────────────────────

ComplexSignal DopplerNipFilter::loadFromCSV(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("DopplerNipFilter::loadFromCSV: не удалось открыть " + filename);

    ComplexSignal result;
    std::string line;
    while (std::getline(file, line)) {
        // Пропускаем комментарии (строки, начинающиеся с #)
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string sRe, sIm;
        if (!std::getline(ss, sRe, ',')) continue;
        if (!std::getline(ss, sIm, ',')) {
            // Только вещественная часть (обратная совместимость)
            result.emplace_back(std::stod(sRe), 0.0);
        } else {
            result.emplace_back(std::stod(sRe), std::stod(sIm));
        }
    }

    return result;
}

void DopplerNipFilter::saveToCSV(const ComplexSignal& signal,
                                  const std::string& filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("DopplerNipFilter::saveToCSV: не удалось открыть " + filename);

    file << std::fixed << std::setprecision(8);
    for (const auto& c : signal)
        file << c.real() << "," << c.imag() << "\n";
}
