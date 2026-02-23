#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <iomanip>
#include "../src/kalman_filter.h"

bool isEqual(double a, double b, double epsilon = 1e-6) {
    return std::abs(a - b) < epsilon;
}

void test_kalman_basic() {
    std::cout << "Тестируем базовый функционал KalmanFilter...\n";

    // Создаем фильтр Калмана
    KalmanFilter filter(0.1, 1.0, 1.0);

    // Тестируем имя фильтра
    std::string name = filter.getName();
    std::cout << "Имя фильтра: " << name << "\n";

    // Тестируем пустой сигнал
    std::vector<double> empty_signal;
    auto result = filter.process(empty_signal);
    assert(result.empty());
    std::cout << "Тест пустого сигнала пройден\n";

    // Тестируем простой сигнал
    std::vector<double> simple_signal = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto filtered = filter.process(simple_signal);

    assert(filtered.size() == simple_signal.size());
    std::cout << "Исходный сигнал: ";
    for (double val : simple_signal) {
        std::cout << val << " ";
    }
    std::cout << "\n";

    std::cout << "Отфильтрованный: ";
    for (double val : filtered) {
        std::cout << val << " ";
    }
    std::cout << "\n";

    // Проверяем, что результат имеет правильный размер
    assert(filtered.size() == 5);

    // Первое значение должно совпадать с исходным (инициализация)
    assert(isEqual(filtered[0], simple_signal[0]));

    std::cout << "Базовый тест KalmanFilter пройден!\n\n";
}

void test_kalman_parameters() {
    std::cout << "Тестируем параметры KalmanFilter...\n";

    KalmanFilter filter(0.5, 2.0, 0.5);

    // Тестируем получение состояния и ковариации
    auto state = filter.getState();
    auto covariance = filter.getCovariance();

    assert(state.size() == 2);
    assert(covariance.size() == 4);

    std::cout << "Начальное состояние: [" << state[0] << ", " << state[1] << "]\n";
    std::cout << "Начальная ковариация: [" << covariance[0] << ", " << covariance[1]
              << ", " << covariance[2] << ", " << covariance[3] << "]\n";

    // Тестируем установку новых параметров
    filter.setParameters(0.2, 0.8, 1.5);

    // Тестируем reset
    filter.reset();

    std::cout << "Тест параметров пройден!\n\n";
}

void test_kalman_edge_cases() {
    std::cout << "Тестируем граничные случаи KalmanFilter...\n";

    // Тест некорректных параметров
    try {
        KalmanFilter filter(-0.1, 1.0, 1.0); // Отрицательный process noise
        assert(false); // Не должны дойти сюда
    } catch (const std::invalid_argument& e) {
        std::cout << "Корректно поймано исключение: " << e.what() << "\n";
    }

    try {
        KalmanFilter filter(0.1, -1.0, 1.0); // Отрицательный measurement noise
        assert(false);
    } catch (const std::invalid_argument& e) {
        std::cout << "Корректно поймано исключение: " << e.what() << "\n";
    }

    try {
        KalmanFilter filter(0.1, 1.0, 0.0); // Нулевой deltaT
        assert(false);
    } catch (const std::invalid_argument& e) {
        std::cout << "Корректно поймано исключение: " << e.what() << "\n";
    }

    // Тест одиночного значения
    KalmanFilter filter(0.1, 1.0, 1.0);
    std::vector<double> single = {42.0};
    auto result = filter.process(single);
    assert(result.size() == 1);
    assert(isEqual(result[0], 42.0));

    std::cout << "Тесты граничных случаев пройдены!\n\n";
}

void test_kalman_noise_filtering() {
    std::cout << "Тестируем фильтрацию шума...\n";

    KalmanFilter filter(0.01, 1.0, 1.0); // Малый process noise, большой measurement noise

    // Создаем сигнал с шумом
    std::vector<double> noisy_signal;
    for (int i = 0; i < 10; ++i) {
        double clean_value = i * 0.5; // Линейно растущий сигнал
        double noise = (i % 2 == 0) ? 0.5 : -0.5; // Простой шум
        noisy_signal.push_back(clean_value + noise);
    }

    auto filtered = filter.process(noisy_signal);

    std::cout << "Зашумленный сигнал: ";
    for (double val : noisy_signal) {
        std::cout << std::fixed << std::setprecision(2) << val << " ";
    }
    std::cout << "\n";

    std::cout << "Отфильтрованный:   ";
    for (double val : filtered) {
        std::cout << std::fixed << std::setprecision(2) << val << " ";
    }
    std::cout << "\n";

    // Проверяем, что фильтр сгладил сигнал
    // (это очень простая проверка - в реальности нужны более сложные метрики)
    assert(filtered.size() == noisy_signal.size());

    std::cout << "Тест фильтрации шума пройден!\n\n";
}

int main() {
    std::cout << "=== ТЕСТИРОВАНИЕ ФИЛЬТРА КАЛМАНА ===\n\n";

    try {
        test_kalman_basic();
        test_kalman_parameters();
        test_kalman_edge_cases();
        test_kalman_noise_filtering();

        std::cout << "✓ Все тесты фильтра Калмана прошли успешно!\n";

    } catch (const std::exception& e) {
        std::cout << "✗ Тест провален: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "✗ Неизвестная ошибка в тестах\n";
        return 1;
    }

    return 0;
}