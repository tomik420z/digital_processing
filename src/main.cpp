#include <iostream>
#include <memory>
#include <vector>
#include <iomanip>
#include <string>

#include "signal_processor.h"
#include "median_filter.h"
#include "wiener_filter.h"
#include "morphological_filter.h"
#include "outlier_detection.h"
#include "savgol_filter.h"
#include "signal_generator.h"
#include "performance_tester.h"

void printHeader() {
    std::cout << "================================================\n";
    std::cout << "  АЛГОРИТМЫ ЗАЩИТЫ ЭХО СИГНАЛОВ ОТ ПОМЕХ\n";
    std::cout << "================================================\n\n";
}

void demonstrateAlgorithms() {
    std::cout << "=== ДЕМОНСТРАЦИЯ РАБОТЫ АЛГОРИТМОВ ===\n\n";

    // Создаем генератор сигналов
    SignalGenerator generator(42);

    // Генерируем тестовый эхо сигнал
    auto cleanSignal = generator.generateEchoSignal(
        SignalGenerator::EchoType::GAUSSIAN, 500, 1.0, 100, 0.6, 0.02
    );

    // Добавляем импульсные помехи
    auto noisySignal = generator.addImpulseNoise(
        cleanSignal, SignalGenerator::NoiseType::RANDOM_SPIKES, 0.02, 2.0
    );

    std::cout << "Сгенерирован тестовый сигнал:\n";
    std::cout << "  Длина: " << cleanSignal.size() << " отсчетов\n";
    std::cout << "  Тип: Гауссовский импульс с эхо\n";
    std::cout << "  Помехи: Случайные выбросы\n\n";

    // Создаем алгоритмы фильтрации
    std::vector<std::unique_ptr<SignalProcessor>> filters;

    filters.push_back(std::make_unique<MedianFilter>(7));
    filters.push_back(std::make_unique<WienerFilter>(8, 0.01, 0.99));
    filters.push_back(std::make_unique<MorphologicalFilter>(
        MorphologicalFilter::Operation::OPENING, 5));
    filters.push_back(std::make_unique<OutlierDetection>(
        OutlierDetection::DetectionMethod::MAD_BASED,
        OutlierDetection::InterpolationMethod::LINEAR, 3.0, 11));

    // Применяем каждый алгоритм и выводим результаты
    for (auto& filter : filters) {
        auto [filteredSignal, executionTime] = filter->measurePerformance(noisySignal);

        double snr = calculateSNR(cleanSignal, filteredSignal);
        double mse = calculateMSE(cleanSignal, filteredSignal);
        double correlation = calculateCorrelation(cleanSignal, filteredSignal);

        std::cout << filter->getName() << ":\n";
        std::cout << "  SNR: " << std::fixed << std::setprecision(2) << snr << " дБ\n";
        std::cout << "  MSE: " << std::scientific << std::setprecision(2) << mse << "\n";
        std::cout << "  Корреляция: " << std::fixed << std::setprecision(3) << correlation << "\n";
        std::cout << "  Время: " << executionTime << " мкс\n\n";
    }
}

void demonstrateBasicSignals() {
    std::cout << "=== ДЕМОНСТРАЦИЯ ОСНОВНЫХ ТИПОВ СИГНАЛОВ ===\n\n";

    // Создаем генератор сигналов
    SignalGenerator generator(42);

    // Параметры для генерации сигналов
    size_t signalLength = 500;
    double amplitude = 1.0;
    double frequency = 0.1;
    double phase = 0.0;
    double dutyCycle = 0.5;

    // Типы сигналов для демонстрации
    std::vector<SignalGenerator::SignalType> signalTypes = {
        SignalGenerator::SignalType::SINE,
        SignalGenerator::SignalType::SQUARE,
        SignalGenerator::SignalType::TRIANGLE,
        SignalGenerator::SignalType::SAWTOOTH
    };

    std::cout << "Генерация и тестирование основных сигналов:\n";
    std::cout << "  Длина: " << signalLength << " отсчетов\n";
    std::cout << "  Амплитуда: " << amplitude << "\n";
    std::cout << "  Частота: " << frequency << "\n\n";

    // Создаем алгоритм для тестирования (медианный фильтр)
    auto filter = std::make_unique<MedianFilter>(7);

    for (auto signalType : signalTypes) {
        // Генерируем чистый сигнал
        auto cleanSignal = generator.generateBasicSignal(
            signalType, signalLength, amplitude, frequency, phase, dutyCycle
        );

        // Добавляем импульсные помехи
        auto noisySignal = generator.addImpulseNoise(
            cleanSignal, SignalGenerator::NoiseType::RANDOM_SPIKES, 0.02, 2.0
        );

        // Применяем фильтрацию
        auto [filteredSignal, executionTime] = filter->measurePerformance(noisySignal);

        // Вычисляем метрики качества
        double snr = calculateSNR(cleanSignal, filteredSignal);
        double mse = calculateMSE(cleanSignal, filteredSignal);
        double correlation = calculateCorrelation(cleanSignal, filteredSignal);

        std::cout << SignalGenerator::signalTypeToString(signalType) << " сигнал:\n";
        std::cout << "  SNR: " << std::fixed << std::setprecision(2) << snr << " дБ\n";
        std::cout << "  MSE: " << std::scientific << std::setprecision(2) << mse << "\n";
        std::cout << "  Корреляция: " << std::fixed << std::setprecision(3) << correlation << "\n";
        std::cout << "  Время фильтрации: " << executionTime << " мкс\n";

        // Сохраняем сигналы для анализа
        std::string signalName = SignalGenerator::signalTypeToString(signalType);
        try {
            std::string rootPath(ROOT_PATH);
            SignalGenerator::saveSignalToCSV(cleanSignal,
                rootPath + "/data/clean/" + signalName + "_clean.csv");
            SignalGenerator::saveSignalToCSV(noisySignal,
                rootPath + "/data/noisy/" + signalName + "_noisy.csv");
            std::cout << "  Сохранено: " + signalName + "_clean.csv и " + signalName + "_noisy.csv\n";
        } catch (const std::exception& e) {
            std::cout << "  Ошибка сохранения: " << e.what() << "\n";
        }
        std::cout << "\n";
    }
}

void runFullBenchmark() {
    std::cout << "=== ПОЛНОЕ ТЕСТИРОВАНИЕ АЛГОРИТМОВ ===\n\n";

    // Создаем тестер производительности
    PerformanceTester tester(42);

    // Добавляем алгоритмы для тестирования
    tester.addAlgorithm(std::make_unique<MedianFilter>(5));
    tester.addAlgorithm(std::make_unique<MedianFilter>(7));
    tester.addAlgorithm(std::make_unique<MedianFilter>(9));

    tester.addAlgorithm(std::make_unique<WienerFilter>(6, 0.01, 0.99));
    tester.addAlgorithm(std::make_unique<WienerFilter>(10, 0.005, 0.995));

    tester.addAlgorithm(std::make_unique<MorphologicalFilter>(
        MorphologicalFilter::Operation::OPENING, 3));
    tester.addAlgorithm(std::make_unique<MorphologicalFilter>(
        MorphologicalFilter::Operation::CLOSING, 5));

    tester.addAlgorithm(std::make_unique<OutlierDetection>(
        OutlierDetection::DetectionMethod::MAD_BASED,
        OutlierDetection::InterpolationMethod::LINEAR, 2.5, 9));
    tester.addAlgorithm(std::make_unique<OutlierDetection>(
        OutlierDetection::DetectionMethod::STATISTICAL,
        OutlierDetection::InterpolationMethod::MEDIAN_BASED, 3.0, 11));
    tester.addAlgorithm(std::make_unique<OutlierDetection>(
        OutlierDetection::DetectionMethod::ADAPTIVE_THRESHOLD,
        OutlierDetection::InterpolationMethod::AUTOREGRESSIVE, 2.0, 7));

    std::cout << "Генерация тестового набора данных...\n";
    tester.generateTestDataset(1000, 30);

    std::cout << "Запуск тестирования...\n\n";
    auto results = tester.runFullTest();

    // Генерируем и выводим отчет
    std::string report = tester.generateReport(results);
    std::cout << report << std::endl;

    // Сохраняем результаты
    try {
        tester.saveResultsToCSV(results, "results/benchmark_results.csv");
        tester.saveTestDataset("data/clean", "data/noisy");
        std::cout << "Результаты сохранены в файлы:\n";
        std::cout << "  - results/benchmark_results.csv\n";
        std::cout << "  - data/clean/ и data/noisy/\n";
    } catch (const std::exception& e) {
        std::cerr << "Ошибка при сохранении: " << e.what() << std::endl;
    }
}

void runScalabilityTest() {
    std::cout << "=== ТЕСТИРОВАНИЕ МАСШТАБИРУЕМОСТИ ===\n\n";

    PerformanceTester tester(42);

    // Добавляем представительные алгоритмы
    tester.addAlgorithm(std::make_unique<MedianFilter>(7));
    tester.addAlgorithm(std::make_unique<WienerFilter>(8, 0.01, 0.99));
    tester.addAlgorithm(std::make_unique<MorphologicalFilter>(
        MorphologicalFilter::Operation::OPENING, 5));
    tester.addAlgorithm(std::make_unique<OutlierDetection>(
        OutlierDetection::DetectionMethod::MAD_BASED,
        OutlierDetection::InterpolationMethod::LINEAR, 3.0, 11));

    // Тестируем на различных размерах сигналов
    std::vector<size_t> signalLengths = {100, 250, 500, 1000, 2000, 4000};

    std::cout << "Тестирование на сигналах длиной: ";
    for (size_t length : signalLengths) {
        std::cout << length << " ";
    }
    std::cout << "отсчетов\n\n";

    auto scalabilityResults = tester.testScalability(signalLengths);

    // Выводим результаты масштабируемости
    std::cout << "Результаты тестирования масштабируемости:\n\n";

    for (const auto& [algorithmName, results] : scalabilityResults) {
        std::cout << algorithmName << ":\n";
        for (const auto& [length, time] : results) {
            std::cout << "  " << length << " отсчетов: "
                      << std::fixed << std::setprecision(0) << time << " мкс\n";
        }
        std::cout << "\n";
    }
}

void showMenu() {
    std::cout << "Выберите режим работы:\n";
    std::cout << "1. Демонстрация алгоритмов\n";
    std::cout << "2. Демонстрация основных типов сигналов\n";
    std::cout << "3. Полное тестирование\n";
    std::cout << "4. Тестирование масштабируемости\n";
    std::cout << "5. Выход\n";
    std::cout << "Ваш выбор: ";
}

int main() {
    printHeader();

    int choice;
    do {
        showMenu();
        std::cin >> choice;
        std::cout << "\n";

        try {
            switch (choice) {
                case 1:
                    demonstrateAlgorithms();
                    break;
                case 2:
                    demonstrateBasicSignals();
                    break;
                case 3:
                    runFullBenchmark();
                    break;
                case 4:
                    runScalabilityTest();
                    break;
                case 5:
                    std::cout << "Программа завершена.\n";
                    break;
                default:
                    std::cout << "Неверный выбор. Попробуйте снова.\n";
                    break;
            }
        } catch (const std::exception& e) {
            std::cerr << "Ошибка: " << e.what() << std::endl;
        }

        if (choice != 5) {
            std::cout << "\nНажмите Enter для продолжения...";
            std::cin.ignore();
            std::cin.get();
            std::cout << "\n";
        }

    } while (choice != 5);

    return 0;
}