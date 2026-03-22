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
