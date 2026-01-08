#ifndef SIGNAL_PROCESSOR_H
#define SIGNAL_PROCESSOR_H

#include <vector>
#include <string>
#include <chrono>

/**
 * Базовый класс для обработки сигналов
 */
class SignalProcessor {
public:
    using Signal = std::vector<double>;

    /**
     * Виртуальный деструктор
     */
    virtual ~SignalProcessor() = default;

    /**
     * Применить фильтр к сигналу
     * @param input Входной сигнал
     * @return Отфильтрованный сигнал
     */
    virtual Signal process(const Signal& input) = 0;

    /**
     * Получить имя алгоритма
     */
    virtual std::string getName() const = 0;

    /**
     * Измерить время выполнения обработки
     * @param input Входной сигнал
     * @return Пара: отфильтрованный сигнал и время выполнения в микросекундах
     */
    std::pair<Signal, long long> measurePerformance(const Signal& input);

protected:
    /**
     * Вычислить медиану вектора значений
     * @param values Вектор значений
     * @return Медиана
     */
    static double median(std::vector<double> values);

    /**
     * Вычислить медианное абсолютное отклонение
     * @param values Вектор значений
     * @param med Медиана значений
     * @return MAD
     */
    static double mad(const std::vector<double>& values, double med);

    /**
     * Линейная интерполяция между двумя точками
     * @param x1, y1 Первая точка
     * @param x2, y2 Вторая точка
     * @param x Точка интерполяции
     * @return Интерполированное значение
     */
    static double linearInterpolate(double x1, double y1, double x2, double y2, double x);
};

/**
 * Структура для хранения результатов тестирования
 */
struct TestResult {
    std::string algorithmName;
    double snr;                    // Отношение сигнал/шум
    double mse;                    // Среднеквадратичная ошибка
    double correlation;            // Коэффициент корреляции
    long long executionTime;       // Время выполнения в микросекундах

    TestResult(const std::string& name = "")
        : algorithmName(name), snr(0.0), mse(0.0), correlation(0.0), executionTime(0) {}
};

/**
 * Вычислить отношение сигнал/шум
 * @param clean Чистый сигнал
 * @param noisy Зашумленный сигнал
 * @return SNR в дБ
 */
double calculateSNR(const SignalProcessor::Signal& clean, const SignalProcessor::Signal& noisy);

/**
 * Вычислить среднеквадратичную ошибку
 * @param original Исходный сигнал
 * @param processed Обработанный сигнал
 * @return MSE
 */
double calculateMSE(const SignalProcessor::Signal& original, const SignalProcessor::Signal& processed);

/**
 * Вычислить коэффициент корреляции
 * @param signal1 Первый сигнал
 * @param signal2 Второй сигнал
 * @return Коэффициент корреляции
 */
double calculateCorrelation(const SignalProcessor::Signal& signal1, const SignalProcessor::Signal& signal2);

#endif // SIGNAL_PROCESSOR_H