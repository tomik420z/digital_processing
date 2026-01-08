#ifndef SIGNAL_GENERATOR_H
#define SIGNAL_GENERATOR_H

#include "signal_processor.h"
#include <random>

/**
 * Генератор тестовых сигналов и помех
 */
class SignalGenerator {
public:
    using Signal = SignalProcessor::Signal;

    /**
     * Типы основных сигналов
     */
    enum class SignalType {
        SINE,           // Синусоидальный сигнал
        SQUARE,         // Квадратный сигнал
        TRIANGLE,       // Треугольный (пилообразный) сигнал
        SAWTOOTH        // Пилообразный сигнал (альтернативная форма)
    };

    /**
     * Типы эхо сигналов
     */
    enum class EchoType {
        RECTANGULAR,    // Прямоугольный импульс
        TRIANGULAR,     // Треугольный импульс
        GAUSSIAN,       // Гауссовский импульс
        EXPONENTIAL,    // Экспоненциальный импульс
        CHIRP          // ЛЧМ (линейно-частотно-модулированный) импульс
    };

    /**
     * Типы импульсных помех
     */
    enum class NoiseType {
        IMPULSE,        // Одиночные импульсы
        BURST,          // Пакетные помехи
        RANDOM_SPIKES,  // Случайные выбросы
        PERIODIC        // Периодические импульсы
    };

private:
    mutable std::mt19937 rng_;  // Генератор случайных чисел

public:
    /**
     * Конструктор
     * @param seed Начальное значение для генератора случайных чисел
     */
    explicit SignalGenerator(unsigned int seed = std::random_device{}());

    /**
     * Генерировать основной сигнал заданного типа
     * @param type Тип сигнала (синус, квадрат, треугольник)
     * @param length Длина сигнала в отсчетах
     * @param amplitude Амплитуда сигнала
     * @param frequency Нормированная частота (0.0-0.5)
     * @param phase Начальная фаза в радианах
     * @param dutyCycle Коэффициент заполнения для квадратного сигнала (0.0-1.0)
     * @return Сгенерированный сигнал
     */
    Signal generateBasicSignal(SignalType type,
                              size_t length,
                              double amplitude = 1.0,
                              double frequency = 0.1,
                              double phase = 0.0,
                              double dutyCycle = 0.5) const;

    /**
     * Генерировать эхо сигнал
     * @param type Тип эхо сигнала
     * @param length Длина сигнала в отсчетах
     * @param amplitude Амплитуда сигнала
     * @param echoDelay Задержка эхо (в отсчетах)
     * @param echoAttenuation Ослабление эхо (0.0-1.0)
     * @param noiseLevel Уровень фонового шума
     * @return Сгенерированный сигнал
     */
    Signal generateEchoSignal(EchoType type,
                              size_t length,
                              double amplitude = 1.0,
                              size_t echoDelay = 100,
                              double echoAttenuation = 0.5,
                              double noiseLevel = 0.01) const;

    /**
     * Генерировать импульсные помехи
     * @param length Длина сигнала помех
     * @param type Тип помех
     * @param density Плотность помех (вероятность появления в каждом отсчете)
     * @param amplitude Амплитуда помех
     * @param burstLength Длина пакета помех (для пакетных помех)
     * @return Сигнал помех
     */
    Signal generateImpulseNoise(size_t length,
                                NoiseType type = NoiseType::RANDOM_SPIKES,
                                double density = 0.01,
                                double amplitude = 2.0,
                                size_t burstLength = 5) const;

    /**
     * Добавить импульсные помехи к сигналу
     * @param signal Исходный сигнал
     * @param noiseType Тип помех
     * @param density Плотность помех
     * @param amplitude Амплитуда помех
     * @return Сигнал с добавленными помехами
     */
    Signal addImpulseNoise(const Signal& signal,
                           NoiseType noiseType = NoiseType::RANDOM_SPIKES,
                           double density = 0.01,
                           double amplitude = 2.0) const;

    /**
     * Генерировать белый шум
     * @param length Длина сигнала
     * @param variance Дисперсия шума
     * @return Сигнал белого шума
     */
    Signal generateWhiteNoise(size_t length, double variance = 1.0) const;

    /**
     * Генерировать тестовый набор данных
     * @param signalLength Длина сигналов
     * @param numSignals Количество различных типов сигналов
     * @return Набор тестовых сигналов (clean, noisy)
     */
    std::vector<std::pair<Signal, Signal>> generateTestDataset(size_t signalLength = 1000,
                                                               size_t numSignals = 10) const;

    /**
     * Сохранить сигнал в CSV файл
     * @param signal Сигнал для сохранения
     * @param filename Имя файла
     */
    static void saveSignalToCSV(const Signal& signal, const std::string& filename);

    /**
     * Загрузить сигнал из CSV файла
     * @param filename Имя файла
     * @return Загруженный сигнал
     */
    static Signal loadSignalFromCSV(const std::string& filename);

    /**
     * Получить строковое представление типа основного сигнала
     */
    static std::string signalTypeToString(SignalType type);

private:
    /**
     * Генерировать базовый импульс заданного типа
     * @param type Тип импульса
     * @param length Длина импульса
     * @param amplitude Амплитуда
     * @return Сгенерированный импульс
     */
    Signal generatePulse(EchoType type, size_t length, double amplitude) const;

    /**
     * Генерировать прямоугольный импульс
     * @param length Длина импульса
     * @param amplitude Амплитуда
     * @return Прямоугольный импульс
     */
    Signal generateRectangularPulse(size_t length, double amplitude) const;

    /**
     * Генерировать треугольный импульс
     * @param length Длина импульса
     * @param amplitude Амплитуда
     * @return Треугольный импульс
     */
    Signal generateTriangularPulse(size_t length, double amplitude) const;

    /**
     * Генерировать гауссовский импульс
     * @param length Длина импульса
     * @param amplitude Амплитуда
     * @return Гауссовский импульс
     */
    Signal generateGaussianPulse(size_t length, double amplitude) const;

    /**
     * Генерировать экспоненциальный импульс
     * @param length Длина импульса
     * @param amplitude Амплитуда
     * @return Экспоненциальный импульс
     */
    Signal generateExponentialPulse(size_t length, double amplitude) const;

    /**
     * Генерировать ЛЧМ импульс
     * @param length Длина импульса
     * @param amplitude Амплитуда
     * @return ЛЧМ импульс
     */
    Signal generateChirpPulse(size_t length, double amplitude) const;

    /**
     * Генерировать синусоидальный сигнал
     * @param length Длина сигнала
     * @param amplitude Амплитуда
     * @param frequency Нормированная частота
     * @param phase Начальная фаза
     * @return Синусоидальный сигнал
     */
    Signal generateSineSignal(size_t length, double amplitude, double frequency, double phase) const;

    /**
     * Генерировать квадратный сигнал
     * @param length Длина сигнала
     * @param amplitude Амплитуда
     * @param frequency Нормированная частота
     * @param phase Начальная фаза
     * @param dutyCycle Коэффициент заполнения
     * @return Квадратный сигнал
     */
    Signal generateSquareSignal(size_t length, double amplitude, double frequency, double phase, double dutyCycle) const;

    /**
     * Генерировать треугольный сигнал
     * @param length Длина сигнала
     * @param amplitude Амплитуда
     * @param frequency Нормированная частота
     * @param phase Начальная фаза
     * @return Треугольный сигнал
     */
    Signal generateTriangleSignal(size_t length, double amplitude, double frequency, double phase) const;

    /**
     * Генерировать пилообразный сигнал
     * @param length Длина сигнала
     * @param amplitude Амплитуда
     * @param frequency Нормированная частота
     * @param phase Начальная фаза
     * @return Пилообразный сигнал
     */
    Signal generateSawtoothSignal(size_t length, double amplitude, double frequency, double phase) const;

    /**
     * Получить строковое представление типа эхо
     */
    static std::string echoTypeToString(EchoType type);

    /**
     * Получить строковое представление типа помех
     */
    static std::string noiseTypeToString(NoiseType type);
};

#endif // SIGNAL_GENERATOR_H