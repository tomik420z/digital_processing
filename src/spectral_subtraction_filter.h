#ifndef SPECTRAL_SUBTRACTION_FILTER_H
#define SPECTRAL_SUBTRACTION_FILTER_H

#include "signal_processor.h"
#include <string>

/**
 * Фильтр спектрального вычитания (Spectral Subtraction Filter).
 *
 * Алгоритм (Boll, 1979 — классическая форма):
 *
 *   1. Оценка спектра шума N̂[k]:
 *      Сигнал разбивается на блоки (кадры) длиной frameSize с перекрытием overlap.
 *      Первые noiseFrames кадров считаются «шумовыми» — по ним вычисляется
 *      усреднённая мощность шума: N̂[k] = (1/M) * Σ |Xᵢ[k]|²
 *      Затем оценка обновляется рекурсивно:
 *          N̂[k] ← (1−μ)·N̂[k] + μ·|X[k]|²   если |X[k]|² < γ·N̂[k]
 *      (тихие участки — обновляем оценку шума, активные — нет).
 *
 *   2. Спектральное вычитание для каждого кадра:
 *      |Ŝ[k]|² = max(|X[k]|² − α·N̂[k],  β·|X[k]|²)
 *
 *      Параметры:
 *        α (subtractionFactor) — насколько агрессивно вычитаем шум (≥ 1.0, типично 2–4)
 *        β (spectralFloor)     — «пол» спектра, предотвращает музыкальные артефакты
 *                                (0.001–0.1, типично 0.002)
 *
 *   3. Восстановление фазы:
 *      Фаза входного сигнала сохраняется без изменений:
 *          Ŝ[k] = |Ŝ[k]| · exp(j·∠X[k])
 *
 *   4. IFFT + Overlap-Add:
 *      Каждый обработанный кадр суммируется в выходной буфер методом overlap-add.
 *
 * Почему это работает для импульсных помех:
 *   Несинхронный импульс даёт широкополосный всплеск во всем спектре,
 *   тогда как полезный сигнал (эхо-импульс, синус) имеет компактный спектр.
 *   Вычитание оценённого «шумового пьедестала» убирает эти всплески.
 *
 * Параметры:
 *   frameSize        — размер FFT-кадра (степень двойки, по умолчанию 256)
 *   hopSize          — шаг между кадрами (frameSize/4, по умолчанию 64)
 *   noiseFrames      — сколько первых кадров использовать для оценки шума (по умолч. 4)
 *   subtractionFactor— коэффициент α вычитания (по умолч. 2.0)
 *   spectralFloor    — нижний предел β (по умолч. 0.002)
 *   noiseUpdateRate  — скорость обновления μ оценки шума (по умолч. 0.1)
 *   noiseThreshold   — порог γ для обновления: обновляем, если мощность < γ·N̂ (по умолч. 1.5)
 */
class SpectralSubtractionFilter : public SignalProcessor {
public:
    /**
     * Конструктор
     * @param frameSize        Размер FFT-кадра (должен быть степенью двойки)
     * @param hopSize          Шаг (сдвиг) между кадрами (0 = frameSize/4)
     * @param noiseFrames      Число начальных кадров для инициализации оценки шума
     * @param subtractionFactor Коэффициент α (насколько агрессивно вычитать шум)
     * @param spectralFloor    Нижний предел β (защита от артефактов)
     * @param noiseUpdateRate  Скорость μ обновления шумовой оценки
     * @param noiseThreshold   Порог γ для обновления оценки шума
     */
    explicit SpectralSubtractionFilter(size_t frameSize        = 256,
                                       size_t hopSize          = 0,
                                       size_t noiseFrames      = 4,
                                       double subtractionFactor = 2.0,
                                       double spectralFloor    = 0.002,
                                       double noiseUpdateRate  = 0.1,
                                       double noiseThreshold   = 1.5);

    /**
     * Применить фильтр спектрального вычитания к сигналу
     * @param input Входной (зашумлённый) сигнал
     * @return Отфильтрованный сигнал
     */
    Signal process(const Signal& input) override;

    /**
     * Получить имя алгоритма
     */
    std::string getName() const override;

    /**
     * Установить параметры
     */
    void setParameters(size_t frameSize,
                       size_t hopSize,
                       size_t noiseFrames,
                       double subtractionFactor,
                       double spectralFloor,
                       double noiseUpdateRate,
                       double noiseThreshold);

private:
    size_t frameSize_;         ///< Размер FFT-кадра
    size_t hopSize_;           ///< Шаг между кадрами
    size_t noiseFrames_;       ///< Число кадров для оценки шума
    double subtractionFactor_; ///< Коэффициент α
    double spectralFloor_;     ///< Нижний предел β
    double noiseUpdateRate_;   ///< Скорость обновления μ
    double noiseThreshold_;    ///< Порог γ обновления шума

    /// Создать окно Ханна длиной n
    static std::vector<double> hannWindow(size_t n);

    /// Проверить и скорректировать параметры
    void validateParams();
};

#endif // SPECTRAL_SUBTRACTION_FILTER_H
