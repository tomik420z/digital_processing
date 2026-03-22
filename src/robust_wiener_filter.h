#ifndef ROBUST_WIENER_FILTER_H
#define ROBUST_WIENER_FILTER_H

#include "signal_processor.h"
#include "outlier_detection.h"

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/io.hpp>

namespace ublas = boost::numeric::ublas;

/**
 * Автоматически оцениваемые параметры робастного фильтра Винера.
 * Заполняется методом RobustWienerFilter::estimateParameters(signal).
 */
struct WienerParams {
    size_t filterOrder;       ///< Порядок FIR-фильтра M (по спектру: 1 / f_signal_95)
    size_t desiredWindow;     ///< Окно скользящей медианы d[n]   (≈ filterOrder / 3)
    double regularization;    ///< Тихоновская регуляризация λ     (≈ σ²_noise)
    double outlierThreshold;  ///< Порог MAD-детектора выбросов    (по SNR-оценке)
    size_t outlierWindow;     ///< Окно MAD-детектора              (≈ filterOrder * 2 + 1)

    /// Диагностика: оценённые характеристики входного сигнала
    double estimatedNoiseSigma;   ///< σ шума (MAD / 0.6745)
    double estimatedSignalRMS;    ///< RMS сигнала
    double estimatedSNR_dB;       ///< Приближённый SNR в дБ
    double dominantFrequency;     ///< Доминирующая нормированная частота сигнала (0..0.5)
};

/**
 * Робастный фильтр Винера для подавления помех, включая несинхронные импульсы.
 *
 * Отличия от классического WienerFilter:
 *
 *   1. МЕДИАННАЯ оценка желаемого сигнала d[n]:
 *      Вместо скользящего среднего (чувствительного к выбросам) используется
 *      скользящая медиана, которая подавляет импульсы до построения весов.
 *
 *   2. ДВУХЭТАПНЫЙ конвейер (OutlierDetection → Винер):
 *      Перед вычислением весов и фильтрацией входной сигнал предварительно
 *      очищается от импульсных выбросов методом MAD + медианная интерполяция.
 *      Фильтр Винера затем работает уже на сигнале без импульсов → не «обучается»
 *      на выбросах и не «вносит» их в автокорреляционную матрицу R.
 *
 *   3. КОРРЕКТНОЕ нулевое дополнение (zero-padding) на краях:
 *      При n < i используется 0.0 вместо x[0], устраняя краевые артефакты
 *      на первых filterOrder_ отсчётах.
 *
 * Математическая основа:
 *   w_opt = R⁻¹ · p,  где:
 *   R[i,j] = (1/K) * Σ xc[n-i] * xc[n-j]   — автокорреляция очищенного сигнала
 *   p[i]   = (1/K) * Σ d[n] * xc[n-i]        — взаимная корреляция (d — медианная оценка)
 *
 * Фильтрация итогового выхода:
 *   y[n] = wᵀ · x[n]   — применяется к ИСХОДНОМУ (не очищенному) сигналу,
 *                         чтобы линейный фильтр дополнительно сглаживал остаточный шум.
 */
class RobustWienerFilter : public SignalProcessor {
public:
    /**
     * Конструктор
     * @param filterOrder    Порядок фильтра M (длина окна весов)
     * @param desiredWindow  Размер окна скользящей медианы для оценки d[n]
     * @param regularization Коэффициент тихоновской регуляризации
     * @param outlierThreshold  Порог MAD для обнаружения импульсных выбросов
     *                          (в единицах MAD, рекомендуется 3.0 – 4.0)
     * @param outlierWindow  Размер окна MAD-детектора (нечётное число)
     */
    explicit RobustWienerFilter(size_t filterOrder       = 10,
                                size_t desiredWindow     = 5,
                                double regularization    = 1e-4,
                                double outlierThreshold  = 3.5,
                                size_t outlierWindow     = 11);

    /**
     * Применить фильтр к сигналу
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
    void setParameters(size_t filterOrder,
                       size_t desiredWindow,
                       double regularization    = 1e-4,
                       double outlierThreshold  = 3.5,
                       size_t outlierWindow     = 11);

    /**
     * Получить вычисленные оптимальные веса w_opt (после вызова process)
     */
    std::vector<double> getWeights() const;

    /**
     * Автоматически оценить параметры фильтра по входному сигналу.
     *
     * Комбинированный подход A+B:
     *   — Вариант A (статистика): оценивает σ_noise через MAD, вычисляет
     *     SNR и на его основе подбирает outlierThreshold и regularization.
     *   — Вариант B (спектр): вычисляет FFT входного сигнала, находит
     *     частоту, ниже которой сосредоточено 95% энергии сигнала (f_95),
     *     и из неё выводит filterOrder = round(1 / (2·f_95)).
     *
     * Алгоритм:
     *   1. Вычислить MAD(x) → σ_noise = MAD / 0.6745
     *   2. Вычислить RMS(x) → SNR_dB = 20·log10(RMS / σ_noise)
     *   3. outlierThreshold: 2.5 если SNR < 5 дБ, 3.5 если 5–15 дБ, 5.0 если > 15 дБ
     *   4. regularization = σ_noise²  (масштабируется с уровнем шума)
     *   5. Вычислить FFT(x), построить спектр мощности
     *   6. Найти f_95 — частота, ниже которой 95% суммарной спектральной мощности
     *   7. filterOrder = clamp(round(1 / (2·f_95)), 4, 128)
     *   8. desiredWindow = clamp(filterOrder / 3, 3, 51)  (нечётное)
     *   9. outlierWindow = clamp(filterOrder * 2 + 1, 7, 101) (нечётное)
     *
     * @param signal  Входной (зашумлённый) сигнал
     * @return        Структура WienerParams с подобранными параметрами
     *                и диагностическими полями
     */
    static WienerParams estimateParameters(const std::vector<double>& signal);

private:
    size_t filterOrder_;      ///< Порядок фильтра M
    size_t desiredWindow_;    ///< Окно скользящей медианы для d[n]
    double regularization_;   ///< Тихоновская регуляризация
    double outlierThreshold_; ///< Порог MAD-детектора выбросов
    size_t outlierWindow_;    ///< Окно MAD-детектора

    ublas::vector<double> weights_; ///< Оптимальные веса w_opt после solve

    /**
     * Шаг 1: предварительная очистка от импульсных выбросов через OutlierDetection
     * (MAD_BASED + MEDIAN_BASED интерполяция)
     */
    Signal removeImpulses(const Signal& x) const;

    /**
     * Шаг 2: оценка желаемого сигнала d[n] — скользящая МЕДИАНА
     * (устойчива к выбросам в отличие от скользящего среднего)
     */
    Signal estimateDesiredMedian(const Signal& x) const;

    /**
     * Построить матрицу R (автокорреляция очищенного сигнала)
     * R[i,j] = (1/K) * Σ_{n=M-1}^{N-1} xc[n-i] * xc[n-j]
     * с нулевым дополнением при выходе за пределы
     */
    ublas::matrix<double> buildCorrelationMatrix(const Signal& xc) const;

    /**
     * Построить вектор p (взаимная корреляция очищенного сигнала и d[n])
     * p[i] = (1/K) * Σ_{n=M-1}^{N-1} d[n] * xc[n-i]
     */
    ublas::vector<double> buildCrossCorrelationVector(const Signal& xc,
                                                      const Signal& d) const;
};

#endif // ROBUST_WIENER_FILTER_H
