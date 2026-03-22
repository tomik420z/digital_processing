#include "spectral_subtraction_filter.h"
#include "utils/fft.h"

#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <numeric>

// ─────────────────────────────────────────────────────────────────────────────
// Конструктор / validateParams
// ─────────────────────────────────────────────────────────────────────────────

SpectralSubtractionFilter::SpectralSubtractionFilter(size_t frameSize,
                                                     size_t hopSize,
                                                     size_t noiseFrames,
                                                     double subtractionFactor,
                                                     double spectralFloor,
                                                     double noiseUpdateRate,
                                                     double noiseThreshold)
    : frameSize_(frameSize),
      hopSize_(hopSize),
      noiseFrames_(noiseFrames),
      subtractionFactor_(subtractionFactor),
      spectralFloor_(spectralFloor),
      noiseUpdateRate_(noiseUpdateRate),
      noiseThreshold_(noiseThreshold)
{
    validateParams();
}

void SpectralSubtractionFilter::validateParams()
{
    if (frameSize_ < 4) frameSize_ = 4;
    if (!fft_impl::isPow2(frameSize_))
        frameSize_ = fft_impl::nextPow2(frameSize_);

    if (hopSize_ == 0 || hopSize_ > frameSize_)
        hopSize_ = frameSize_ / 4;

    if (noiseFrames_ == 0) noiseFrames_ = 1;

    if (subtractionFactor_ <= 0.0) subtractionFactor_ = 1.0;
    if (spectralFloor_ <= 0.0)     spectralFloor_     = 1e-6;
    if (spectralFloor_ >= 1.0)     spectralFloor_     = 0.5;

    if (noiseUpdateRate_ < 0.0) noiseUpdateRate_ = 0.0;
    if (noiseUpdateRate_ > 1.0) noiseUpdateRate_ = 1.0;
    if (noiseThreshold_ <= 1.0) noiseThreshold_ = 1.5;
}

void SpectralSubtractionFilter::setParameters(size_t frameSize,
                                               size_t hopSize,
                                               size_t noiseFrames,
                                               double subtractionFactor,
                                               double spectralFloor,
                                               double noiseUpdateRate,
                                               double noiseThreshold)
{
    frameSize_         = frameSize;
    hopSize_           = hopSize;
    noiseFrames_       = noiseFrames;
    subtractionFactor_ = subtractionFactor;
    spectralFloor_     = spectralFloor;
    noiseUpdateRate_   = noiseUpdateRate;
    noiseThreshold_    = noiseThreshold;
    validateParams();
}

std::string SpectralSubtractionFilter::getName() const
{
    return "SpectralSubtraction_fs" + std::to_string(frameSize_) +
           "_alpha" + std::to_string(static_cast<int>(subtractionFactor_ * 10));
}

// ─────────────────────────────────────────────────────────────────────────────
// Окно Ханна: w[n] = 0.5·(1 − cos(2π·n/(N−1)))
// Использует вариант N−1 в знаменателе → w[0]=w[N-1]=0 (периодическое окно)
// ─────────────────────────────────────────────────────────────────────────────

std::vector<double> SpectralSubtractionFilter::hannWindow(size_t n)
{
    std::vector<double> w(n);
    const double denom = (n > 1) ? static_cast<double>(n - 1) : 1.0;
    for (size_t i = 0; i < n; ++i)
        w[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(i) / denom));
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// Основная функция: Weighted Overlap-Add (WOLA) + спектральное вычитание
//
// Исправленные критические точки по сравнению с первой версией:
//   1. Кадры фазы инициализации ВСЕГДА добавляются в выходной буфер
//      (pass-through без вычитания, пока не накоплена оценка шума).
//   2. spectralFloor теперь абсолютный: floor = β·N̂[k]  (не β·|X[k]|²).
//      Это предотвращает усиление шума на «тихих» бинах.
//   3. noisePow хранит оценку БЕЗ деления на winEnergy — корректная
//      единица «мощность в бине FFT».
//   4. Адаптивное обновление оценки шума работает только тогда, когда
//      полная мощность кадра ниже γ × среднешумовой мощности.
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal SpectralSubtractionFilter::process(const Signal& input)
{
    const size_t N       = input.size();
    const size_t fftSize = frameSize_;
    const size_t hop     = hopSize_;

    if (N == 0) return Signal();

    // Для сигналов короче одного кадра — дополняем нулями
    if (N < fftSize) {
        Signal padded(fftSize, 0.0);
        std::copy(input.begin(), input.end(), padded.begin());
        Signal res = process(padded);
        res.resize(N);
        return res;
    }

    // ── Окно Ханна ────────────────────────────────────────────────────────────
    const std::vector<double> win = hannWindow(fftSize);

    // Вычисляем нормирующую сумму COLA: при 75%-перекрытии для окна Ханна
    // сумма w²[n] по всем перекрывающимся кадрам ≈ const.
    // Буфер нормализатора сам посчитает правильную сумму.

    // ── Буферы Overlap-Add ────────────────────────────────────────────────────
    const size_t outLen = N + fftSize;
    Signal output(outLen, 0.0);
    Signal normalizer(outLen, 0.0);

    // ── Оценка шума: накапливаем N̂[k] по первым noiseFrames_ кадрам ──────────
    std::vector<double> noisePow(fftSize, 0.0);
    bool   noiseReady = false;
    size_t noiseCount = 0;

    // ── Двухпроходная обработка ───────────────────────────────────────────────
    // Проход 1: собираем все кадры в массив spectra, одновременно оцениваем шум
    // Проход 2: применяем вычитание и Overlap-Add
    //
    // Чтобы не хранить все кадры, делаем всё в один проход,
    // но кадры фазы инициализации добавляются в выход без изменений.

    for (size_t start = 0; start + fftSize <= N + hop; start += hop) {

        // ── Извлекаем кадр с оконным взвешиванием ────────────────────────────
        CVector frame(fftSize, Complex(0.0, 0.0));
        for (size_t i = 0; i < fftSize; ++i) {
            const size_t idx = start + i;
            const double val = (idx < N) ? input[idx] : 0.0;
            frame[i] = Complex(val * win[i], 0.0);
        }

        // ── FFT ──────────────────────────────────────────────────────────────
        fft_impl::fft_inplace(frame, false);

        // ── Вычисляем мощность текущего кадра (сумма |X[k]|²) ────────────────
        double framePow = 0.0;
        for (size_t k = 0; k < fftSize; ++k)
            framePow += std::norm(frame[k]);
        framePow /= static_cast<double>(fftSize);

        // ── Фаза накопления шума ──────────────────────────────────────────────
        if (!noiseReady) {
            for (size_t k = 0; k < fftSize; ++k)
                noisePow[k] += std::norm(frame[k]);
            ++noiseCount;

            if (noiseCount >= noiseFrames_) {
                // Нормируем — получаем среднюю мощность на бин
                const double inv = 1.0 / static_cast<double>(noiseCount);
                for (size_t k = 0; k < fftSize; ++k)
                    noisePow[k] *= inv;
                noiseReady = true;
            }

            // ── FIX 1: добавляем кадр в выход WITHOUT вычитания ──────────────
            // (pass-through для первых noiseFrames_ кадров)
            fft_impl::fft_inplace(frame, true); // IFFT
            for (size_t i = 0; i < fftSize; ++i) {
                const size_t outIdx = start + i;
                if (outIdx < outLen) {
                    output[outIdx]     += frame[i].real() * win[i];
                    normalizer[outIdx] += win[i] * win[i];
                }
            }
            continue; // переходим к следующему кадру
        }

        // ── Адаптивное обновление оценки шума ────────────────────────────────
        // Средняя шумовая мощность на бин
        double meanNoisePow = 0.0;
        for (size_t k = 0; k < fftSize; ++k)
            meanNoisePow += noisePow[k];
        meanNoisePow /= static_cast<double>(fftSize);

        // Обновляем только если кадр не содержит активного сигнала
        // (мощность кадра ≤ γ × средняя шумовая мощность)
        if (framePow <= noiseThreshold_ * meanNoisePow) {
            for (size_t k = 0; k < fftSize; ++k) {
                const double pow_k = std::norm(frame[k]);
                noisePow[k] = (1.0 - noiseUpdateRate_) * noisePow[k]
                               + noiseUpdateRate_ * pow_k;
            }
        }

        // ── Спектральное вычитание ────────────────────────────────────────────
        // |Ŝ[k]|² = max(|X[k]|² − α·N̂[k],  β·N̂[k])
        //
        // FIX 2: floor = β·N̂[k]  (абсолютный, привязан к уровню шума)
        //        а не β·|X[k]|² (что усиливало шум на тихих бинах)
        for (size_t k = 0; k < fftSize; ++k) {
            const double mag2    = std::norm(frame[k]);
            const double noise_k = noisePow[k];

            // Абсолютный пол: β × мощность шума в данном бине
            const double floor_k = spectralFloor_ * noise_k;
            const double sub     = mag2 - subtractionFactor_ * noise_k;
            const double newMag2 = std::max(sub, floor_k);
            const double newMag  = std::sqrt(std::max(newMag2, 0.0));

            // Масштабируем вектор с сохранением фазы
            if (mag2 > 1e-30) {
                const double origMag = std::sqrt(mag2);
                frame[k] *= (newMag / origMag);
            } else {
                frame[k] = Complex(0.0, 0.0);
            }
        }

        // ── IFFT ──────────────────────────────────────────────────────────────
        fft_impl::fft_inplace(frame, true);

        // ── Overlap-Add ───────────────────────────────────────────────────────
        for (size_t i = 0; i < fftSize; ++i) {
            const size_t outIdx = start + i;
            if (outIdx < outLen) {
                output[outIdx]     += frame[i].real() * win[i];
                normalizer[outIdx] += win[i] * win[i];
            }
        }
    }

    // ── WOLA-нормировка и обрезка до исходной длины ───────────────────────────
    Signal result(N);
    for (size_t i = 0; i < N; ++i) {
        result[i] = (normalizer[i] > 1e-12)
                    ? output[i] / normalizer[i]
                    : 0.0;
    }

    return result;
}
