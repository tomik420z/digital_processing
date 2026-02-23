#include "kalman_filter.h"
#include <stdexcept>
#include <cmath>

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
            x_ = Vector2D(input[i], 0.0);  // [позиция, скорость]

            // Инициализация ковариационной матрицы
            P_ = Matrix2x2::identity();

            initialized_ = true;
            output.push_back(input[i]);
        } else {
            // Шаг предсказания
            predict();

            // Шаг коррекции
            update(input[i]);

            // Выходное значение - позиция (первый элемент вектора состояния)
            output.push_back(x_.x());
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
    x_ = Vector2D(0.0, 0.0);

    // Сброс ковариационной матрицы
    P_ = Matrix2x2::identity();
}

std::vector<double> KalmanFilter::getState() const {
    return x_.toVector();
}

std::vector<double> KalmanFilter::getCovariance() const {
    return {P_(0,0), P_(0,1), P_(1,0), P_(1,1)};
}

void KalmanFilter::initializeMatrices() {
    // Матрица перехода состояния F (модель постоянной скорости)
    // x(k+1) = [1 dt] * [x(k)]   + w(k)
    //          [0  1]   [v(k)]
    F_ = Matrix2x2(1.0, deltaT_, 0.0, 1.0);

    // Матрица наблюдения H (измеряем только позицию)
    // z(k) = [1 0] * [x(k)] + v(k)
    //                 [v(k)]
    H_ = Vector2D(1.0, 0.0);

    // Ковариационная матрица шума процесса Q
    // Используем модель белого шума ускорения
    double dt2 = deltaT_ * deltaT_;
    double dt3 = dt2 * deltaT_;
    double dt4 = dt3 * deltaT_;

    Q_ = Matrix2x2(
        processNoise_ * dt4 / 4.0,  // дисперсия позиции
        processNoise_ * dt3 / 2.0,  // ковариация позиция-скорость
        processNoise_ * dt3 / 2.0,  // ковариация скорость-позиция
        processNoise_ * dt2         // дисперсия скорости
    );
}

void KalmanFilter::predict() {
    // Предсказание состояния: x_pred = F * x
    Vector2D x_pred = Vector2D(F_ * x_.toVector());

    // Предсказание ковариационной матрицы: P_pred = F * P * F^T + Q
    Matrix2x2 P_pred = F_ * P_ * F_.transpose() + Q_;

    // Обновляем состояние
    x_ = x_pred;
    P_ = P_pred;
}

void KalmanFilter::update(double measurement) {
    // Невязка: y = z - H * x
    double innovation = measurement - H_.dot(x_);

    // Ковариация невязки: S = H * P * H^T + R
    std::vector<double> H_P = P_ * H_.toVector();
    double S = H_.dot(Vector2D(H_P)) + measurementNoise_;

    if (std::abs(S) < 1e-12) {
        // Избегаем деления на ноль
        return;
    }

    // Коэффициент усиления Калмана: K = P * H^T / S
    std::vector<double> P_H = P_ * H_.toVector();
    Vector2D K = Vector2D(P_H) * (1.0 / S);

    // Коррекция состояния: x = x + K * y
    x_ = x_ + K * innovation;

    // Обновление ковариационной матрицы: P = (I - K * H) * P
    Matrix2x2 I_KH = Matrix2x2::identity() -
                     Matrix2x2(K[0] * H_[0], K[0] * H_[1],
                              K[1] * H_[0], K[1] * H_[1]);

    P_ = I_KH * P_;
}

std::vector<double> KalmanFilter::matrixVectorMultiply(const std::vector<std::vector<double>>& matrix,
                                                      const std::vector<double>& vector) const {
    // Этот метод больше не используется, но оставляем для совместимости
    Matrix2x2 m(matrix);
    return m * vector;
}

std::vector<std::vector<double>> KalmanFilter::matrixMultiply(const std::vector<std::vector<double>>& A,
                                                             const std::vector<std::vector<double>>& B) const {
    // Этот метод больше не используется, но оставляем для совместимости
    Matrix2x2 matA(A);
    Matrix2x2 matB(B);
    return (matA * matB).toVector();
}

std::vector<std::vector<double>> KalmanFilter::matrixTranspose(const std::vector<std::vector<double>>& matrix) const {
    // Этот метод больше не используется, но оставляем для совместимости
    Matrix2x2 m(matrix);
    return m.transpose().toVector();
}

std::vector<std::vector<double>> KalmanFilter::matrixAdd(const std::vector<std::vector<double>>& A,
                                                        const std::vector<std::vector<double>>& B) const {
    // Этот метод больше не используется, но оставляем для совместимости
    Matrix2x2 matA(A);
    Matrix2x2 matB(B);
    return (matA + matB).toVector();
}

std::vector<std::vector<double>> KalmanFilter::matrixSubtract(const std::vector<std::vector<double>>& A,
                                                             const std::vector<std::vector<double>>& B) const {
    // Этот метод больше не используется, но оставляем для совместимости
    Matrix2x2 matA(A);
    Matrix2x2 matB(B);
    return (matA - matB).toVector();
}

double KalmanFilter::dotProduct(const std::vector<double>& a, const std::vector<double>& b) const {
    // Этот метод больше не используется, но оставляем для совместимости
    Vector2D vecA(a);
    Vector2D vecB(b);
    return vecA.dot(vecB);
}

std::vector<std::vector<double>> KalmanFilter::matrixInverse2x2(const std::vector<std::vector<double>>& matrix) const {
    // Этот метод больше не используется, но оставляем для совместимости
    Matrix2x2 m(matrix);
    return m.inverse().toVector();
}