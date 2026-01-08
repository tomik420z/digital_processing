#ifndef PERFORMANCE_TESTER_H
#define PERFORMANCE_TESTER_H

#include "signal_processor.h"
#include "signal_generator.h"
#include <memory>
#include <vector>
#include <map>

/**
 * Система тестирования и сравнения алгоритмов фильтрации
 */
class PerformanceTester {
public:
    using Signal = SignalProcessor::Signal;

    /**
     * Структура для хранения детальных результатов теста
     */
    struct DetailedTestResult {
        std::string algorithmName;
        std::vector<double> snrResults;      // SNR для каждого тестового сигнала
        std::vector<double> mseResults;      // MSE для каждого тестового сигнала
        std::vector<double> correlationResults; // Корреляция для каждого тестового сигнала
        std::vector<long long> executionTimes;  // Время выполнения для каждого сигнала

        // Статистические показатели
        double avgSNR;
        double avgMSE;
        double avgCorrelation;
        double avgExecutionTime;
        double stdSNR;
        double stdMSE;
        double stdCorrelation;
        double stdExecutionTime;

        DetailedTestResult(const std::string& name = "") : algorithmName(name) {}
    };

private:
    SignalGenerator generator_;
    std::vector<std::unique_ptr<SignalProcessor>> algorithms_;
    std::vector<std::pair<Signal, Signal>> testDataset_; // (clean, noisy) пары

public:
    /**
     * Конструктор
     * @param seed Начальное значение для генератора случайных чисел
     */
    explicit PerformanceTester(unsigned int seed = 42);

    /**
     * Добавить алгоритм для тестирования
     * @param algorithm Умный указатель на алгоритм
     */
    void addAlgorithm(std::unique_ptr<SignalProcessor> algorithm);

    /**
     * Генерировать тестовый набор данных
     * @param signalLength Длина каждого сигнала
     * @param numSignals Количество тестовых сигналов
     * @param customNoiseParams Пользовательские параметры помех (опционально)
     */
    void generateTestDataset(size_t signalLength = 1000,
                            size_t numSignals = 50);

    /**
     * Загрузить тестовый набор данных из файлов
     * @param cleanSignalsDir Директория с чистыми сигналами
     * @param noisySignalsDir Директория с зашумленными сигналами
     */
    void loadTestDataset(const std::string& cleanSignalsDir,
                        const std::string& noisySignalsDir);

    /**
     * Запустить полное тестирование всех алгоритмов
     * @return Детальные результаты для каждого алгоритма
     */
    std::vector<DetailedTestResult> runFullTest();

    /**
     * Тестировать один алгоритм на всем наборе данных
     * @param algorithm Алгоритм для тестирования
     * @return Детальные результаты теста
     */
    DetailedTestResult testAlgorithm(SignalProcessor& algorithm);

    /**
     * Сравнить два алгоритма
     * @param algorithm1 Первый алгоритм
     * @param algorithm2 Второй алгоритм
     * @return Результаты сравнения
     */
    std::map<std::string, double> compareAlgorithms(SignalProcessor& algorithm1,
                                                   SignalProcessor& algorithm2);

    /**
     * Генерировать отчет о тестировании
     * @param results Результаты тестирования
     * @return Отчет в текстовом формате
     */
    std::string generateReport(const std::vector<DetailedTestResult>& results) const;

    /**
     * Сохранить результаты в CSV файл
     * @param results Результаты тестирования
     * @param filename Имя файла для сохранения
     */
    void saveResultsToCSV(const std::vector<DetailedTestResult>& results,
                         const std::string& filename) const;

    /**
     * Сохранить тестовый набор данных
     * @param cleanDir Директория для чистых сигналов
     * @param noisyDir Директория для зашумленных сигналов
     */
    void saveTestDataset(const std::string& cleanDir,
                        const std::string& noisyDir) const;

    /**
     * Получить статистику по тестовому набору
     * @return Статистическая информация о данных
     */
    std::map<std::string, double> getDatasetStatistics() const;

    /**
     * Тестирование масштабируемости алгоритмов
     * @param signalLengths Различные размеры сигналов для тестирования
     * @return Результаты тестирования масштабируемости
     */
    std::map<std::string, std::vector<std::pair<size_t, double>>>
    testScalability(const std::vector<size_t>& signalLengths);

private:
    /**
     * Вычислить статистические показатели
     * @param values Вектор значений
     * @return Пара (среднее, стандартное отклонение)
     */
    std::pair<double, double> calculateStatistics(const std::vector<double>& values) const;

    /**
     * Вычислить статистические показатели для времени выполнения
     * @param values Вектор значений времени
     * @return Пара (среднее, стандартное отклонение)
     */
    std::pair<double, double> calculateTimeStatistics(const std::vector<long long>& values) const;

    /**
     * Создать директорию если она не существует
     * @param path Путь к директории
     */
    void createDirectoryIfNotExists(const std::string& path) const;

    /**
     * Получить список файлов в директории
     * @param directory Путь к директории
     * @return Список имен файлов
     */
    std::vector<std::string> getFilesInDirectory(const std::string& directory) const;
};

#endif // PERFORMANCE_TESTER_H