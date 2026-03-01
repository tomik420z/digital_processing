#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H

#include "signal_processor.h"
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <vector>

namespace ublas = boost::numeric::ublas;

/**
 * Фильтр Калмана для сглаживания одномерных сигналов
 *
 * Реализует дискретный фильтр Калмана с моделью постоянной скорости.
 * Вектор состояния: [позиция, скорость]
 *
 * Модель системы:
 * x(k+1) = F * x(k) + w(k)
 * z(k) = H * x(k) + v(k)
 *
 * где:
 * - F - матрица перехода состояния (2x2)
 * - H - матрица наблюдения (1x2)
 * - w(k) - шум процесса с ковариацией Q (2x2)
 * - v(k) - шум измерения с дисперсией R (скаляр)
 */
class KalmanFilter : public SignalProcessor {
public:
    /**
     * Конструктор
     * @param processNoise Дисперсия шума процесса
     * @param measurementNoise Дисперсия шума измерения
     * @param deltaT Временной интервал между измерениями
     */
    explicit KalmanFilter(double processNoise = 0.1,
                         double measurementNoise = 1.0,
                         double deltaT = 1.0);

    /**
     * Обработать входной сигнал
     * @param input Входной сигнал
     * @return Отфильтрованный сигнал
     */
    Signal process(const Signal& input) override;

    /**
     * Получить имя фильтра
     * @return Строковое представление имени фильтра
     */
    std::string getName() const override;

    /**
     * Установить параметры фильтра
     * @param processNoise Дисперсия шума процесса
     * @param measurementNoise Дисперсия шума измерения
     * @param deltaT Временной интервал между измерениями
     */
    void setParameters(double processNoise, double measurementNoise, double deltaT = 1.0);

    /**
     * Сбросить состояние фильтра
     */
    void reset();

    /**
     * Получить текущее состояние фильтра
     * @return Вектор состояния [позиция, скорость]
     */
    std::vector<double> getState() const;

    /**
     * Получить ковариационную матрицу ошибки
     * @return Матрица P (2x2) в виде вектора [P00, P01, P10, P11]
     */
    std::vector<double> getCovariance() const;

private:
    // Параметры фильтра
    double processNoise_;      // Дисперсия шума процесса (Q)
    double measurementNoise_;  // Дисперсия шума измерения (R)
    double deltaT_;           // Временной интервал

    // Состояние фильтра
    ublas::vector<double> x_;         // Вектор состояния [позиция, скорость]
    ublas::matrix<double> P_;         // Ковариационная матрица ошибки (2x2)

    // Матрицы модели
    ublas::matrix<double> F_;         // Матрица перехода состояния (2x2)
    ublas::vector<double> H_;         // Матрица наблюдения (1x2)
    ublas::matrix<double> Q_;         // Ковариационная матрица шума процесса (2x2)

    bool initialized_;                // Флаг инициализации

    /**
     * Инициализировать матрицы модели
     */
    void initializeMatrices();

    /**
     * Шаг предсказания фильтра Калмана
     */
    void predict();

    /**
     * Шаг коррекции фильтра Калмана
     * @param measurement Измерение (наблюдение)
     */
    void update(double measurement);

};

#endif // KALMAN_FILTER_H