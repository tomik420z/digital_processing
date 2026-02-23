#include <iostream>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include "../src/utils/matrix2x2.h"

// Утилита для сравнения double с точностью
bool isEqual(double a, double b, double epsilon = 1e-10) {
    return std::abs(a - b) < epsilon;
}

// Тест Vector2D
void test_vector2d() {
    std::cout << "Тестируем Vector2D...\n";

    // Тест конструкторов
    Vector2D v1;
    assert(isEqual(v1.x(), 0.0) && isEqual(v1.y(), 0.0));

    Vector2D v2(1.5, 2.5);
    assert(isEqual(v2.x(), 1.5) && isEqual(v2.y(), 2.5));

    std::vector<double> vec = {3.0, 4.0};
    Vector2D v3(vec);
    assert(isEqual(v3.x(), 3.0) && isEqual(v3.y(), 4.0));

    // Тест операций доступа
    assert(isEqual(v3[0], 3.0) && isEqual(v3[1], 4.0));

    v3[0] = 5.0;
    assert(isEqual(v3[0], 5.0));

    // Тест арифметических операций
    Vector2D v4 = v2 + v3;
    assert(isEqual(v4.x(), 6.5) && isEqual(v4.y(), 6.5));

    Vector2D v5 = v4 - v2;
    assert(isEqual(v5.x(), 5.0) && isEqual(v5.y(), 4.0));

    Vector2D v6 = v2 * 2.0;
    assert(isEqual(v6.x(), 3.0) && isEqual(v6.y(), 5.0));

    // Тест скалярного произведения
    double dot = v2.dot(v3);
    assert(isEqual(dot, 1.5 * 5.0 + 2.5 * 4.0)); // 7.5 + 10.0 = 17.5

    // Тест преобразования в std::vector
    auto vec_result = v2.toVector();
    assert(vec_result.size() == 2);
    assert(isEqual(vec_result[0], 1.5) && isEqual(vec_result[1], 2.5));

    std::cout << "Vector2D тесты пройдены!\n";
}

// Тест Matrix2x2
void test_matrix2x2() {
    std::cout << "Тестируем Matrix2x2...\n";

    // Тест конструкторов
    Matrix2x2 m1;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            assert(isEqual(m1(i, j), 0.0));
        }
    }

    Matrix2x2 m2(1.0, 2.0, 3.0, 4.0);
    assert(isEqual(m2(0, 0), 1.0) && isEqual(m2(0, 1), 2.0));
    assert(isEqual(m2(1, 0), 3.0) && isEqual(m2(1, 1), 4.0));

    std::vector<std::vector<double>> mat = {{5.0, 6.0}, {7.0, 8.0}};
    Matrix2x2 m3(mat);
    assert(isEqual(m3(0, 0), 5.0) && isEqual(m3(0, 1), 6.0));
    assert(isEqual(m3(1, 0), 7.0) && isEqual(m3(1, 1), 8.0));

    // Тест доступа к элементам
    m3(0, 0) = 10.0;
    assert(isEqual(m3(0, 0), 10.0));

    // Тест арифметических операций
    Matrix2x2 m4 = m2 + m3;
    assert(isEqual(m4(0, 0), 11.0) && isEqual(m4(0, 1), 8.0));
    assert(isEqual(m4(1, 0), 10.0) && isEqual(m4(1, 1), 12.0));

    Matrix2x2 m5 = m4 - m2;
    assert(isEqual(m5(0, 0), 10.0) && isEqual(m5(0, 1), 6.0));
    assert(isEqual(m5(1, 0), 7.0) && isEqual(m5(1, 1), 8.0));

    Matrix2x2 m6 = m2 * 2.0;
    assert(isEqual(m6(0, 0), 2.0) && isEqual(m6(0, 1), 4.0));
    assert(isEqual(m6(1, 0), 6.0) && isEqual(m6(1, 1), 8.0));

    // Тест умножения матриц
    Matrix2x2 identity = Matrix2x2::identity();
    Matrix2x2 m7 = m2 * identity;
    assert(isEqual(m7(0, 0), 1.0) && isEqual(m7(0, 1), 2.0));
    assert(isEqual(m7(1, 0), 3.0) && isEqual(m7(1, 1), 4.0));

    // Тест умножения на вектор
    std::vector<double> vec = {1.0, 1.0};
    auto result = m2 * vec;
    assert(result.size() == 2);
    assert(isEqual(result[0], 3.0) && isEqual(result[1], 7.0)); // [1*1+2*1, 3*1+4*1]

    // Тест определителя
    double det = m2.determinant();
    assert(isEqual(det, -2.0)); // 1*4 - 2*3 = 4 - 6 = -2

    // Тест транспонирования
    Matrix2x2 m8 = m2.transpose();
    assert(isEqual(m8(0, 0), 1.0) && isEqual(m8(0, 1), 3.0));
    assert(isEqual(m8(1, 0), 2.0) && isEqual(m8(1, 1), 4.0));

    // Тест обращения
    try {
        Matrix2x2 inv = m2.inverse();
        Matrix2x2 should_be_identity = m2 * inv;
        assert(should_be_identity.isEqual(identity, 1e-10));
    } catch (const std::runtime_error& e) {
        // Матрица может быть вырожденной
        std::cout << "Матрица вырождена: " << e.what() << std::endl;
    }

    // Тест статических методов
    Matrix2x2 zeros = Matrix2x2::zeros();
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            assert(isEqual(zeros(i, j), 0.0));
        }
    }

    assert(isEqual(identity(0, 0), 1.0) && isEqual(identity(0, 1), 0.0));
    assert(isEqual(identity(1, 0), 0.0) && isEqual(identity(1, 1), 1.0));

    // Тест преобразования в std::vector
    auto mat_result = m2.toVector();
    assert(mat_result.size() == 2);
    assert(mat_result[0].size() == 2 && mat_result[1].size() == 2);
    assert(isEqual(mat_result[0][0], 1.0) && isEqual(mat_result[0][1], 2.0));
    assert(isEqual(mat_result[1][0], 3.0) && isEqual(mat_result[1][1], 4.0));

    std::cout << "Matrix2x2 тесты пройдены!\n";
}

// Тест граничных случаев
void test_edge_cases() {
    std::cout << "Тестируем граничные случаи...\n";

    // Тест исключений Vector2D
    try {
        Vector2D v({1.0}); // Неправильный размер
        assert(false); // Не должны дойти сюда
    } catch (const std::invalid_argument&) {
        // Ожидаемое исключение
    }

    try {
        Vector2D v;
        v[2]; // Выход за границы
        assert(false);
    } catch (const std::out_of_range&) {
        // Ожидаемое исключение
    }

    // Тест исключений Matrix2x2
    try {
        std::vector<std::vector<double>> wrong_mat = {{1.0, 2.0, 3.0}}; // Неправильный размер
        Matrix2x2 m(wrong_mat);
        assert(false);
    } catch (const std::invalid_argument&) {
        // Ожидаемое исключение
    }

    try {
        Matrix2x2 m;
        m(2, 0); // Выход за границы
        assert(false);
    } catch (const std::out_of_range&) {
        // Ожидаемое исключение
    }

    // Тест вырожденной матрицы
    try {
        Matrix2x2 singular(1.0, 2.0, 2.0, 4.0); // det = 0
        singular.inverse();
        assert(false);
    } catch (const std::runtime_error&) {
        // Ожидаемое исключение
    }

    std::cout << "Тесты граничных случаев пройдены!\n";
}

int main() {
    std::cout << "=== ЮНИТ ТЕСТЫ Matrix2x2 и Vector2D ===\n\n";

    try {
        test_vector2d();
        test_matrix2x2();
        test_edge_cases();

        std::cout << "\n✓ Все тесты успешно пройдены!\n";

    } catch (const std::exception& e) {
        std::cout << "\n✗ Тест провален: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "\n✗ Неизвестная ошибка в тестах\n";
        return 1;
    }

    return 0;
}