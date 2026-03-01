#ifndef WIENER_FILTER_H
#define WIENER_FILTER_H

#include "signal_processor.h"

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/io.hpp>

namespace ublas = boost::numeric::ublas;

/**
 * Фильтр Винера для подавления помех.
 *
 * Реализует классическое матричное решение:
 *   w_opt = R⁻¹ · p
 *
 * где:
 *   R — автокорреляционная матрица входного сигнала (M×M),
 *       R[i,j] = (1/N) * Σ x[n-i] * x[n-j]
 *   p — вектор взаимной корреляции между входным и желаемым сигналом (M),
 *       p[i] = (1/N) * Σ d[n] * x[n-i]
 *
 * Желаемый сигнал d[n] оценивается как скользящее среднее входного
 * (предположение: истинный сигнал — низкочастотный, помехи — высокочастотные).
 *
 * Матрица R инвертируется через LU-разложение (boost::numeric::ublas::lu_factorize).
 */
class WienerFilter : public SignalProcessor {
public:
    /**
     * Конструктор
     * @param filterOrder Порядок фильтра M (длина окна весов)
     * @param desiredWindow Размер окна скользящего среднего для оценки d[n]
     * @param regularization Коэффициент регуляризации (добавляется к диагонали R)
     */
    explicit WienerFilter(size_t filterOrder    = 10,
                          size_t desiredWindow  = 5,
                          double regularization = 1e-4);

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
     * @param filterOrder Порядок фильтра
     * @param desiredWindow Окно оценки желаемого сигнала
     * @param regularization Коэффициент регуляризации
     */
    void setParameters(size_t filterOrder,
                       size_t desiredWindow,
                       double regularization = 1e-4);

    /**
     * Получить вычисленные оптимальные веса w_opt (после вызова process)
     */
    std::vector<double> getWeights() const;

private:
    size_t filterOrder_;    ///< Порядок фильтра M
    size_t desiredWindow_;  ///< Окно скользящего среднего для d[n]
    double regularization_; ///< Тихоновская регуляризация (диагональное добавление к R)

    ublas::vector<double> weights_; ///< Оптимальные веса w_opt после solve

    /**
     * Построить матрицу R (автокорреляция входного сигнала)
     * R[i,j] = (1/N) * Σ_{n=M-1}^{N-1} x[n-i] * x[n-j]
     */
    ublas::matrix<double> buildCorrelationMatrix(const Signal& x) const;

    /**
     * Построить вектор p (взаимная корреляция входного и желаемого)
     * p[i] = (1/N) * Σ_{n=M-1}^{N-1} d[n] * x[n-i]
     */
    ublas::vector<double> buildCrossCorrelationVector(const Signal& x,
                                                      const Signal& d) const;

    /**
     * Оценить желаемый сигнал d[n] как скользящее среднее x[n]
     */
    Signal estimateDesired(const Signal& x) const;
};

#endif // WIENER_FILTER_H
