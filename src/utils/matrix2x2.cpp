#include "matrix2x2.h"
#include <stdexcept>
#include <cmath>
#include <iomanip>

// ============================================================================
// Реализация Matrix2x2
// ============================================================================

Matrix2x2::Matrix2x2() {
    data_[0][0] = 0.0;
    data_[0][1] = 0.0;
    data_[1][0] = 0.0;
    data_[1][1] = 0.0;
}

Matrix2x2::Matrix2x2(double a00, double a01, double a10, double a11) {
    data_[0][0] = a00;
    data_[0][1] = a01;
    data_[1][0] = a10;
    data_[1][1] = a11;
}

Matrix2x2::Matrix2x2(const std::vector<std::vector<double>>& matrix) {
    if (matrix.size() != 2 || matrix[0].size() != 2 || matrix[1].size() != 2) {
        throw std::invalid_argument("Matrix must be 2x2");
    }

    data_[0][0] = matrix[0][0];
    data_[0][1] = matrix[0][1];
    data_[1][0] = matrix[1][0];
    data_[1][1] = matrix[1][1];
}

double& Matrix2x2::operator()(size_t row, size_t col) {
    if (row >= 2 || col >= 2) {
        throw std::out_of_range("Matrix indices out of range");
    }
    return data_[row][col];
}

const double& Matrix2x2::operator()(size_t row, size_t col) const {
    if (row >= 2 || col >= 2) {
        throw std::out_of_range("Matrix indices out of range");
    }
    return data_[row][col];
}

Matrix2x2 Matrix2x2::operator+(const Matrix2x2& other) const {
    return Matrix2x2(
        data_[0][0] + other.data_[0][0], data_[0][1] + other.data_[0][1],
        data_[1][0] + other.data_[1][0], data_[1][1] + other.data_[1][1]
    );
}

Matrix2x2 Matrix2x2::operator-(const Matrix2x2& other) const {
    return Matrix2x2(
        data_[0][0] - other.data_[0][0], data_[0][1] - other.data_[0][1],
        data_[1][0] - other.data_[1][0], data_[1][1] - other.data_[1][1]
    );
}

Matrix2x2 Matrix2x2::operator*(const Matrix2x2& other) const {
    return Matrix2x2(
        data_[0][0] * other.data_[0][0] + data_[0][1] * other.data_[1][0],
        data_[0][0] * other.data_[0][1] + data_[0][1] * other.data_[1][1],
        data_[1][0] * other.data_[0][0] + data_[1][1] * other.data_[1][0],
        data_[1][0] * other.data_[0][1] + data_[1][1] * other.data_[1][1]
    );
}

Matrix2x2 Matrix2x2::operator*(double scalar) const {
    return Matrix2x2(
        data_[0][0] * scalar, data_[0][1] * scalar,
        data_[1][0] * scalar, data_[1][1] * scalar
    );
}

Matrix2x2& Matrix2x2::operator=(const Matrix2x2& other) {
    if (this != &other) {
        data_[0][0] = other.data_[0][0];
        data_[0][1] = other.data_[0][1];
        data_[1][0] = other.data_[1][0];
        data_[1][1] = other.data_[1][1];
    }
    return *this;
}

std::vector<double> Matrix2x2::operator*(const std::vector<double>& vector) const {
    if (vector.size() != 2) {
        throw std::invalid_argument("Vector must have 2 elements");
    }

    return {
        data_[0][0] * vector[0] + data_[0][1] * vector[1],
        data_[1][0] * vector[0] + data_[1][1] * vector[1]
    };
}

Matrix2x2 Matrix2x2::transpose() const {
    return Matrix2x2(
        data_[0][0], data_[1][0],
        data_[0][1], data_[1][1]
    );
}

Matrix2x2 Matrix2x2::inverse() const {
    double det = determinant();

    if (std::abs(det) < 1e-12) {
        throw std::runtime_error("Matrix is singular, cannot compute inverse");
    }

    double invDet = 1.0 / det;

    return Matrix2x2(
        data_[1][1] * invDet, -data_[0][1] * invDet,
        -data_[1][0] * invDet, data_[0][0] * invDet
    );
}

double Matrix2x2::determinant() const {
    return data_[0][0] * data_[1][1] - data_[0][1] * data_[1][0];
}

Matrix2x2 Matrix2x2::identity() {
    return Matrix2x2(1.0, 0.0, 0.0, 1.0);
}

Matrix2x2 Matrix2x2::zeros() {
    return Matrix2x2(0.0, 0.0, 0.0, 0.0);
}

std::vector<std::vector<double>> Matrix2x2::toVector() const {
    return {
        {data_[0][0], data_[0][1]},
        {data_[1][0], data_[1][1]}
    };
}

std::ostream& operator<<(std::ostream& os, const Matrix2x2& matrix) {
    os << std::fixed << std::setprecision(6);
    os << "[" << std::setw(10) << matrix.data_[0][0] << " " << std::setw(10) << matrix.data_[0][1] << "]\n";
    os << "[" << std::setw(10) << matrix.data_[1][0] << " " << std::setw(10) << matrix.data_[1][1] << "]";
    return os;
}

bool Matrix2x2::isEqual(const Matrix2x2& other, double epsilon) const {
    return (std::abs(data_[0][0] - other.data_[0][0]) < epsilon) &&
           (std::abs(data_[0][1] - other.data_[0][1]) < epsilon) &&
           (std::abs(data_[1][0] - other.data_[1][0]) < epsilon) &&
           (std::abs(data_[1][1] - other.data_[1][1]) < epsilon);
}

// ============================================================================
// Реализация Vector2D
// ============================================================================

Vector2D::Vector2D() {
    data_[0] = 0.0;
    data_[1] = 0.0;
}

Vector2D::Vector2D(double x, double y) {
    data_[0] = x;
    data_[1] = y;
}

Vector2D::Vector2D(const std::vector<double>& vector) {
    if (vector.size() != 2) {
        throw std::invalid_argument("Vector must have 2 elements");
    }

    data_[0] = vector[0];
    data_[1] = vector[1];
}

double& Vector2D::operator[](size_t index) {
    if (index >= 2) {
        throw std::out_of_range("Vector index out of range");
    }
    return data_[index];
}

const double& Vector2D::operator[](size_t index) const {
    if (index >= 2) {
        throw std::out_of_range("Vector index out of range");
    }
    return data_[index];
}

Vector2D Vector2D::operator+(const Vector2D& other) const {
    return Vector2D(data_[0] + other.data_[0], data_[1] + other.data_[1]);
}

Vector2D Vector2D::operator-(const Vector2D& other) const {
    return Vector2D(data_[0] - other.data_[0], data_[1] - other.data_[1]);
}

Vector2D Vector2D::operator*(double scalar) const {
    return Vector2D(data_[0] * scalar, data_[1] * scalar);
}

double Vector2D::dot(const Vector2D& other) const {
    return data_[0] * other.data_[0] + data_[1] * other.data_[1];
}

std::vector<double> Vector2D::toVector() const {
    return {data_[0], data_[1]};
}

std::ostream& operator<<(std::ostream& os, const Vector2D& vector) {
    os << std::fixed << std::setprecision(6);
    os << "[" << std::setw(10) << vector.data_[0] << "]\n";
    os << "[" << std::setw(10) << vector.data_[1] << "]";
    return os;
}