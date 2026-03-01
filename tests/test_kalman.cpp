#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <iomanip>
#include "../src/kalman_filter.h"

// Утилита для сравнения double с точностью
bool isEqual(double a, double b, double epsilon = 1e-6) {
    return std::abs(a - b) < epsilon;
}

// Fixture класс для тестов фильтра Калмана
class KalmanFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Настройка выполняется перед каждым тестом
        filter_ = std::make_unique<KalmanFilter>(0.1, 1.0, 1.0);
    }

    void TearDown() override {
        // Очистка выполняется после каждого теста
        filter_.reset();
    }

    std::unique_ptr<KalmanFilter> filter_;
};

// Тесты базового функционала
TEST_F(KalmanFilterTest, BasicFunctionality) {
    // Тестируем имя фильтра
    std::string name = filter_->getName();
    EXPECT_EQ(name, "KalmanFilter_100_1000_1000");

    // Тестируем пустой сигнал
    std::vector<double> empty_signal;
    auto result = filter_->process(empty_signal);
    EXPECT_TRUE(result.empty());
}

TEST_F(KalmanFilterTest, SimpleSignalProcessing) {
    // Тестируем простой сигнал
    std::vector<double> simple_signal = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto filtered = filter_->process(simple_signal);

    // Проверяем размер результата
    ASSERT_EQ(filtered.size(), simple_signal.size());

    // Первое значение должно совпадать с исходным (инициализация)
    EXPECT_TRUE(isEqual(filtered[0], simple_signal[0]));

    // Проверяем, что фильтр действительно сглаживает сигнал
    for (size_t i = 1; i < filtered.size(); ++i) {
        EXPECT_GT(filtered[i], 0.0);  // Все значения положительны
        EXPECT_LT(filtered[i], 10.0); // Разумные пределы
    }
}

TEST_F(KalmanFilterTest, StateAndCovarianceRetrieval) {
    // Тестируем получение состояния и ковариации
    auto state = filter_->getState();
    auto covariance = filter_->getCovariance();

    ASSERT_EQ(state.size(), 2);
    ASSERT_EQ(covariance.size(), 4);

    // Начальное состояние должно быть нулевым
    EXPECT_TRUE(isEqual(state[0], 0.0));
    EXPECT_TRUE(isEqual(state[1], 0.0));

    // Начальная ковариационная матрица должна быть единичной
    EXPECT_TRUE(isEqual(covariance[0], 1.0)); // P[0,0]
    EXPECT_TRUE(isEqual(covariance[1], 0.0)); // P[0,1]
    EXPECT_TRUE(isEqual(covariance[2], 0.0)); // P[1,0]
    EXPECT_TRUE(isEqual(covariance[3], 1.0)); // P[1,1]
}

TEST_F(KalmanFilterTest, ParameterSetting) {
    // Тестируем установку новых параметров
    EXPECT_NO_THROW(filter_->setParameters(0.2, 0.8, 1.5));

    // Тестируем reset
    EXPECT_NO_THROW(filter_->reset());

    // После reset состояние должно быть нулевым
    auto state = filter_->getState();
    EXPECT_TRUE(isEqual(state[0], 0.0));
    EXPECT_TRUE(isEqual(state[1], 0.0));
}

// Тесты граничных случаев и ошибок
TEST(KalmanFilterErrorTest, InvalidParameters) {
    // Тест отрицательного process noise
    EXPECT_THROW(KalmanFilter filter(-0.1, 1.0, 1.0), std::invalid_argument);

    // Тест отрицательного measurement noise
    EXPECT_THROW(KalmanFilter filter(0.1, -1.0, 1.0), std::invalid_argument);

    // Тест нулевого deltaT
    EXPECT_THROW(KalmanFilter filter(0.1, 1.0, 0.0), std::invalid_argument);

    // Тест отрицательного deltaT
    EXPECT_THROW(KalmanFilter filter(0.1, 1.0, -1.0), std::invalid_argument);
}

TEST(KalmanFilterErrorTest, InvalidParameterSetting) {
    KalmanFilter filter(0.1, 1.0, 1.0);

    // Тест установки некорректных параметров
    EXPECT_THROW(filter.setParameters(-0.1, 1.0, 1.0), std::invalid_argument);
    EXPECT_THROW(filter.setParameters(0.1, -1.0, 1.0), std::invalid_argument);
    EXPECT_THROW(filter.setParameters(0.1, 1.0, 0.0), std::invalid_argument);
}

TEST_F(KalmanFilterTest, SingleValueSignal) {
    // Тест одиночного значения
    std::vector<double> single = {42.0};
    auto result = filter_->process(single);

    ASSERT_EQ(result.size(), 1);
    EXPECT_TRUE(isEqual(result[0], 42.0));
}

// Тест фильтрации зашумленного сигнала
TEST_F(KalmanFilterTest, NoiseFiltering) {
    // Создаем фильтр с малым process noise для лучшего сглаживания
    KalmanFilter noise_filter(0.01, 1.0, 1.0);

    // Создаем сигнал с шумом
    std::vector<double> noisy_signal;
    for (int i = 0; i < 10; ++i) {
        double clean_value = i * 0.5; // Линейно растущий сигнал
        double noise = (i % 2 == 0) ? 0.5 : -0.5; // Простой шум
        noisy_signal.push_back(clean_value + noise);
    }

    auto filtered = noise_filter.process(noisy_signal);

    // Проверяем, что фильтр сгладил сигнал
    ASSERT_EQ(filtered.size(), noisy_signal.size());

    // Первое значение совпадает
    EXPECT_TRUE(isEqual(filtered[0], noisy_signal[0]));

    // Проверяем, что отфильтрованный сигнал менее зашумлен
    // Вычисляем дисперсию исходного и отфильтрованного сигналов
    double noisy_variance = 0.0, filtered_variance = 0.0;
    double noisy_mean = 0.0, filtered_mean = 0.0;

    // Вычисляем средние значения
    for (size_t i = 0; i < noisy_signal.size(); ++i) {
        noisy_mean += noisy_signal[i];
        filtered_mean += filtered[i];
    }
    noisy_mean /= noisy_signal.size();
    filtered_mean /= filtered.size();

    // Вычисляем дисперсии
    for (size_t i = 0; i < noisy_signal.size(); ++i) {
        noisy_variance += (noisy_signal[i] - noisy_mean) * (noisy_signal[i] - noisy_mean);
        filtered_variance += (filtered[i] - filtered_mean) * (filtered[i] - filtered_mean);
    }
    noisy_variance /= noisy_signal.size();
    filtered_variance /= filtered.size();

    // Отфильтрованный сигнал должен иметь меньшую дисперсию (быть более гладким)
    EXPECT_LT(filtered_variance, noisy_variance);
}

// Тест производительности
TEST_F(KalmanFilterTest, Performance) {
    // Создаем большой сигнал
    std::vector<double> large_signal(1000);
    for (size_t i = 0; i < large_signal.size(); ++i) {
        large_signal[i] = std::sin(i * 0.01) + 0.1 * (rand() / double(RAND_MAX));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto result = filter_->process(large_signal);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Проверяем корректность результата
    ASSERT_EQ(result.size(), large_signal.size());

    // Производительность - должно работать быстро
    EXPECT_LT(duration.count(), 100); // Менее 100мс для 1000 точек

    std::cout << "Обработка " << large_signal.size()
              << " точек заняла " << duration.count() << " мс" << std::endl;
}

// Тест разных конфигураций параметров
TEST(KalmanFilterParametersTest, DifferentConfigurations) {
    // Тест с высоким process noise (более адаптивный фильтр)
    KalmanFilter adaptive_filter(1.0, 0.1, 1.0);

    // Тест с низким process noise (более консервативный фильтр)
    KalmanFilter conservative_filter(0.001, 2.0, 1.0);

    // Тест сигнала
    std::vector<double> test_signal = {0.0, 1.0, 0.0, 1.0, 0.0};

    auto adaptive_result = adaptive_filter.process(test_signal);
    auto conservative_result = conservative_filter.process(test_signal);

    ASSERT_EQ(adaptive_result.size(), test_signal.size());
    ASSERT_EQ(conservative_result.size(), test_signal.size());

    // Адаптивный фильтр должен быстрее реагировать на изменения
    EXPECT_GT(adaptive_result[1], conservative_result[1]);
}

// Тест стабильности фильтра
TEST_F(KalmanFilterTest, Stability) {
    // Создаем постоянный сигнал
    std::vector<double> constant_signal(100, 5.0);

    auto result = filter_->process(constant_signal);

    ASSERT_EQ(result.size(), constant_signal.size());

    // После достаточного количества итераций результат должен сходиться к константе
    for (size_t i = 50; i < result.size(); ++i) {
        EXPECT_NEAR(result[i], 5.0, 0.1); // Допускаем погрешность 0.1
    }
}