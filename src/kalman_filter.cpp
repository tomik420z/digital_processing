#include "kalman_filter.h"
#include <boost/numeric/ublas/lu.hpp>
#include <stdexcept>
#include <cmath>

using namespace boost::numeric::ublas;

KalmanFilter::KalmanFilter(double processNoise, double measurementNoise, double deltaT)
    : processNoise_(processNoise),
      measurementNoise_(measurementNoise),
      deltaT_(deltaT),
      initialized_(false) {

    if (processNoise <= 0.0) {
        throw std::invalid_argument("Process noise must be positive");
    }
    if (measurementNoise <= 0.0) {
        throw std::invalid_argument("Measurement noise must be positive");
    }
    if (deltaT <= 0.0) {
        throw std::invalid_argument("Time delta must be positive");
    }

    // Инициализация векторов и матриц с правильными размерами
    x_.resize(2);
    P_.resize(2, 2);
    F_.resize(2, 2);
    H_.resize(2);
    Q_.resize(2, 2);

    initializeMatrices();
    reset();
}

SignalProcessor::Signal KalmanFilter::process(const Signal& input) {
    if (input.empty()) {
        return Signal();
    }

    Signal output;
    output.reserve(input.size());

    // Сброс состояния для каждого нового сигнала
    reset();

    for (size_t i = 0; i < input.size(); ++i) {
        if (!initialized_) {
            // Инициализация состояния первым измерением
            x_(0) = input[i];  // позиция
            x_(1) = 0.0;       // скорость

            // Инициализация ковариационной матрицы
            P_ = identity_matrix<double>(2);

            initialized_ = true;
            output.push_back(input[i]);
        } else {
            // Шаг предсказания
            predict();

            // Шаг коррекции
            update(input[i]);

            // Выходное значение - позиция (первый элемент вектора состояния)
            output.push_back(x_(0));
        }
    }

    return output;
}

std::string KalmanFilter::getName() const {
    return "KalmanFilter_" +
           std::to_string(static_cast<int>(processNoise_ * 1000)) + "_" +
           std::to_string(static_cast<int>(measurementNoise_ * 1000)) + "_" +
           std::to_string(static_cast<int>(deltaT_ * 1000));
}

void KalmanFilter::setParameters(double processNoise, double measurementNoise, double deltaT) {
    if (processNoise <= 0.0) {
        throw std::invalid_argument("Process noise must be positive");
    }
    if (measurementNoise <= 0.0) {
        throw std::invalid_argument("Measurement noise must be positive");
    }
    if (deltaT <= 0.0) {
        throw std::invalid_argument("Time delta must be positive");
    }

    processNoise_ = processNoise;
    measurementNoise_ = measurementNoise;
    deltaT_ = deltaT;

    initializeMatrices();
    reset();
}

void KalmanFilter::reset() {
    initialized_ = false;

    // Сброс вектора состояния
    x_(0) = 0.0;
    x_(1) = 0.0;

    // Сброс ковариационной матрицы
    P_ = identity_matrix<double>(2);
}

std::vector<double> KalmanFilter::getState() const {
    return {x_(0), x_(1)};
}

std::vector<double> KalmanFilter::getCovariance() const {
    return {P_(0,0), P_(0,1), P_(1,0), P_(1,1)};
}

void KalmanFilter::initializeMatrices() {
    // Матрица перехода состояния F (модель постоянной скорости)
    // x(k+1) = [1 dt] * [x(k)]   + w(k)
    //          [0  1]   [v(k)]
    F_(0,0) = 1.0;
    F_(0,1) = deltaT_;
    F_(1,0) = 0.0;
    F_(1,1) = 1.0;

    // Матрица наблюдения H (измеряем только позицию)
    // z(k) = [1 0] * [x(k)] + v(k)
    //                 [v(k)]
    H_(0) = 1.0;
    H_(1) = 0.0;

    // Ковариационная матрица шума процесса Q
    // Используем модель белого шума ускорения
    double dt2 = deltaT_ * deltaT_;
    double dt3 = dt2 * deltaT_;
    double dt4 = dt3 * deltaT_;

    Q_(0,0) = processNoise_ * dt4 / 4.0;  // дисперсия позиции
    Q_(0,1) = processNoise_ * dt3 / 2.0;  // ковариация позиция-скорость
    Q_(1,0) = processNoise_ * dt3 / 2.0;  // ковариация скорость-позиция
    Q_(1,1) = processNoise_ * dt2;        // дисперсия скорости
}

void KalmanFilter::predict() {
    // Предсказание состояния: x_pred = F * x
    x_ = prod(F_, x_);

    // Предсказание ковариационной матрицы: P_pred = F * P * F^T + Q
    matrix<double> F_T = trans(F_);
    P_ = prod(matrix<double>(prod(F_, P_)), F_T) + Q_;
}

void KalmanFilter::update(double measurement) {
    // Невязка: y = z - H * x
    double innovation = measurement - inner_prod(H_, x_);

    // Ковариация невязки: S = H * P * H^T + R
    vector<double> H_P = prod(H_, P_);
    double S = inner_prod(H_, H_P) + measurementNoise_;

    if (std::abs(S) < 1e-12) {
        // Избегаем деления на ноль
        return;
    }

    // Коэффициент усиления Калмана: K = P * H^T / S
    vector<double> P_H = prod(P_, H_);
    vector<double> K = P_H / S;

    // Коррекция состояния: x = x + K * y
    x_ += K * innovation;

    // Обновление ковариационной матрицы: P = (I - K * H) * P
    matrix<double> I = identity_matrix<double>(2);
    matrix<double> K_outer_H = outer_prod(K, H_);
    matrix<double> I_KH = I - K_outer_H;

    P_ = prod(I_KH, P_);
}