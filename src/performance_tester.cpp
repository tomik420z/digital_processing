#include "performance_tester.h"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <sys/stat.h>
#include <dirent.h>
#include <iostream>
#include <sstream>

PerformanceTester::PerformanceTester(unsigned int seed) : generator_(seed) {
}

void PerformanceTester::addAlgorithm(std::unique_ptr<SignalProcessor> algorithm) {
    algorithms_.push_back(std::move(algorithm));
}

void PerformanceTester::generateTestDataset(size_t signalLength, size_t numSignals) {
    testDataset_ = generator_.generateTestDataset(signalLength, numSignals);
}

void PerformanceTester::loadTestDataset(const std::string& cleanSignalsDir,
                                        const std::string& noisySignalsDir) {
    testDataset_.clear();

    auto cleanFiles = getFilesInDirectory(cleanSignalsDir);
    auto noisyFiles = getFilesInDirectory(noisySignalsDir);

    // Сортируем файлы для правильного соответствия
    std::sort(cleanFiles.begin(), cleanFiles.end());
    std::sort(noisyFiles.begin(), noisyFiles.end());

    size_t numFiles = std::min(cleanFiles.size(), noisyFiles.size());

    for (size_t i = 0; i < numFiles; ++i) {
        try {
            Signal cleanSignal = SignalGenerator::loadSignalFromCSV(
                cleanSignalsDir + "/" + cleanFiles[i]
            );
            Signal noisySignal = SignalGenerator::loadSignalFromCSV(
                noisySignalsDir + "/" + noisyFiles[i]
            );

            if (cleanSignal.size() == noisySignal.size() && !cleanSignal.empty()) {
                testDataset_.emplace_back(cleanSignal, noisySignal);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error loading signals from " << cleanFiles[i]
                      << " and " << noisyFiles[i] << ": " << e.what() << std::endl;
        }
    }
}

std::vector<PerformanceTester::DetailedTestResult> PerformanceTester::runFullTest() {
    std::vector<DetailedTestResult> results;
    results.reserve(algorithms_.size());

    for (auto& algorithm : algorithms_) {
        DetailedTestResult result = testAlgorithm(*algorithm);
        results.push_back(result);

        std::cout << "Завершено тестирование алгоритма: " << result.algorithmName
                  << " (SNR: " << std::fixed << std::setprecision(2)
                  << result.avgSNR << " dB)" << std::endl;
    }

    return results;
}

PerformanceTester::DetailedTestResult PerformanceTester::testAlgorithm(SignalProcessor& algorithm) {
    DetailedTestResult result(algorithm.getName());

    result.snrResults.reserve(testDataset_.size());
    result.mseResults.reserve(testDataset_.size());
    result.correlationResults.reserve(testDataset_.size());
    result.executionTimes.reserve(testDataset_.size());

    for (const auto& [cleanSignal, noisySignal] : testDataset_) {
        // Измеряем производительность и применяем фильтр
        auto [filteredSignal, executionTime] = algorithm.measurePerformance(noisySignal);

        // Вычисляем метрики качества
        double snr = calculateSNR(cleanSignal, filteredSignal);
        double mse = calculateMSE(cleanSignal, filteredSignal);
        double correlation = calculateCorrelation(cleanSignal, filteredSignal);

        // Сохраняем результаты
        result.snrResults.push_back(snr);
        result.mseResults.push_back(mse);
        result.correlationResults.push_back(correlation);
        result.executionTimes.push_back(executionTime);
    }

    // Вычисляем статистические показатели
    auto [avgSNR, stdSNR] = calculateStatistics(result.snrResults);
    auto [avgMSE, stdMSE] = calculateStatistics(result.mseResults);
    auto [avgCorrelation, stdCorrelation] = calculateStatistics(result.correlationResults);
    auto [avgExecutionTime, stdExecutionTime] = calculateTimeStatistics(result.executionTimes);

    result.avgSNR = avgSNR;
    result.stdSNR = stdSNR;
    result.avgMSE = avgMSE;
    result.stdMSE = stdMSE;
    result.avgCorrelation = avgCorrelation;
    result.stdCorrelation = stdCorrelation;
    result.avgExecutionTime = avgExecutionTime;
    result.stdExecutionTime = stdExecutionTime;

    return result;
}

std::map<std::string, double> PerformanceTester::compareAlgorithms(SignalProcessor& algorithm1,
                                                                   SignalProcessor& algorithm2) {
    DetailedTestResult result1 = testAlgorithm(algorithm1);
    DetailedTestResult result2 = testAlgorithm(algorithm2);

    std::map<std::string, double> comparison;

    // Разность в качестве (положительные значения означают, что первый алгоритм лучше)
    comparison["SNR_Difference"] = result1.avgSNR - result2.avgSNR;
    comparison["MSE_Ratio"] = result2.avgMSE / result1.avgMSE; // >1 означает, что первый лучше
    comparison["Correlation_Difference"] = result1.avgCorrelation - result2.avgCorrelation;

    // Разность в скорости (отрицательные значения означают, что первый алгоритм быстрее)
    comparison["Speed_Ratio"] = static_cast<double>(result1.avgExecutionTime) /
                               static_cast<double>(result2.avgExecutionTime);

    // Общий индекс качества (комбинированная метрика)
    double quality1 = result1.avgSNR + result1.avgCorrelation - std::log10(result1.avgMSE);
    double quality2 = result2.avgSNR + result2.avgCorrelation - std::log10(result2.avgMSE);
    comparison["Quality_Index_Difference"] = quality1 - quality2;

    return comparison;
}

std::string PerformanceTester::generateReport(const std::vector<DetailedTestResult>& results) const {
    std::stringstream report;

    report << "=== ОТЧЕТ О ТЕСТИРОВАНИИ АЛГОРИТМОВ ЗАЩИТЫ ОТ ИМПУЛЬСНЫХ ПОМЕХ ===\n\n";
    report << "Количество тестовых сигналов: " << testDataset_.size() << "\n";

    auto datasetStats = getDatasetStatistics();
    report << "Статистика тестового набора:\n";
    report << "  Средняя длина сигнала: " << static_cast<int>(datasetStats.at("avg_length")) << "\n";
    report << "  Средний уровень помех: " << std::fixed << std::setprecision(4)
           << datasetStats.at("avg_noise_level") << "\n\n";

    // Таблица результатов
    report << std::left << std::setw(25) << "Алгоритм"
           << std::setw(12) << "SNR (дБ)"
           << std::setw(12) << "MSE"
           << std::setw(12) << "Корреляция"
           << std::setw(15) << "Время (мкс)" << "\n";
    report << std::string(80, '-') << "\n";

    for (const auto& result : results) {
        report << std::left << std::setw(25) << result.algorithmName
               << std::setw(12) << std::fixed << std::setprecision(2) << result.avgSNR
               << std::setw(12) << std::scientific << std::setprecision(2) << result.avgMSE
               << std::setw(12) << std::fixed << std::setprecision(3) << result.avgCorrelation
               << std::setw(15) << std::fixed << std::setprecision(0) << result.avgExecutionTime
               << "\n";
    }

    report << "\n=== ДЕТАЛЬНЫЕ РЕЗУЛЬТАТЫ ===\n\n";

    for (const auto& result : results) {
        report << "Алгоритм: " << result.algorithmName << "\n";
        report << "  SNR: " << std::fixed << std::setprecision(2)
               << result.avgSNR << " ± " << result.stdSNR << " дБ\n";
        report << "  MSE: " << std::scientific << std::setprecision(2)
               << result.avgMSE << " ± " << result.stdMSE << "\n";
        report << "  Корреляция: " << std::fixed << std::setprecision(3)
               << result.avgCorrelation << " ± " << result.stdCorrelation << "\n";
        report << "  Время выполнения: " << std::fixed << std::setprecision(0)
               << result.avgExecutionTime << " ± " << result.stdExecutionTime << " мкс\n\n";
    }

    // Рекомендации
    report << "=== РЕКОМЕНДАЦИИ ===\n\n";

    if (!results.empty()) {
        // Лучший по SNR
        auto bestSNR = std::max_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.avgSNR < b.avgSNR; });

        // Самый быстрый
        auto fastest = std::min_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.avgExecutionTime < b.avgExecutionTime; });

        // Лучший по корреляции
        auto bestCorr = std::max_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.avgCorrelation < b.avgCorrelation; });

        report << "• Лучшее качество (SNR): " << bestSNR->algorithmName << "\n";
        report << "• Самый быстрый: " << fastest->algorithmName << "\n";
        report << "• Лучшая корреляция: " << bestCorr->algorithmName << "\n";
    }

    return report.str();
}

void PerformanceTester::saveResultsToCSV(const std::vector<DetailedTestResult>& results,
                                          const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    // Заголовок
    file << "Algorithm,Avg_SNR,Std_SNR,Avg_MSE,Std_MSE,Avg_Correlation,Std_Correlation,"
         << "Avg_ExecutionTime,Std_ExecutionTime\n";

    // Данные
    for (const auto& result : results) {
        file << result.algorithmName << ","
             << result.avgSNR << "," << result.stdSNR << ","
             << result.avgMSE << "," << result.stdMSE << ","
             << result.avgCorrelation << "," << result.stdCorrelation << ","
             << result.avgExecutionTime << "," << result.stdExecutionTime << "\n";
    }

    file.close();
}

void PerformanceTester::saveTestDataset(const std::string& cleanDir,
                                        const std::string& noisyDir) const {
    createDirectoryIfNotExists(cleanDir);
    createDirectoryIfNotExists(noisyDir);

    for (size_t i = 0; i < testDataset_.size(); ++i) {
        std::string cleanFilename = cleanDir + "/clean_signal_" + std::to_string(i) + ".csv";
        std::string noisyFilename = noisyDir + "/noisy_signal_" + std::to_string(i) + ".csv";

        SignalGenerator::saveSignalToCSV(testDataset_[i].first, cleanFilename);
        SignalGenerator::saveSignalToCSV(testDataset_[i].second, noisyFilename);
    }
}

std::map<std::string, double> PerformanceTester::getDatasetStatistics() const {
    std::map<std::string, double> stats;

    if (testDataset_.empty()) {
        return stats;
    }

    // Средняя длина сигналов
    double avgLength = 0.0;
    for (const auto& [clean, noisy] : testDataset_) {
        avgLength += static_cast<double>(clean.size());
    }
    avgLength /= testDataset_.size();
    stats["avg_length"] = avgLength;

    // Средний уровень помех (RMS разности между чистым и зашумленным)
    double totalNoiseLevel = 0.0;
    for (const auto& [clean, noisy] : testDataset_) {
        double noiseEnergy = 0.0;
        for (size_t i = 0; i < clean.size(); ++i) {
            double noise = noisy[i] - clean[i];
            noiseEnergy += noise * noise;
        }
        totalNoiseLevel += std::sqrt(noiseEnergy / clean.size());
    }
    stats["avg_noise_level"] = totalNoiseLevel / testDataset_.size();

    return stats;
}

std::map<std::string, std::vector<std::pair<size_t, double>>>
PerformanceTester::testScalability(const std::vector<size_t>& signalLengths) {

    std::map<std::string, std::vector<std::pair<size_t, double>>> scalabilityResults;

    // Сохраняем оригинальный тестовый набор
    auto originalDataset = testDataset_;

    for (size_t length : signalLengths) {
        // Генерируем данные для текущей длины
        generateTestDataset(length, 10); // Используем меньше сигналов для ускорения

        // Тестируем каждый алгоритм
        for (auto& algorithm : algorithms_) {
            auto result = testAlgorithm(*algorithm);
            scalabilityResults[result.algorithmName].emplace_back(length, result.avgExecutionTime);
        }
    }

    // Восстанавливаем оригинальный набор
    testDataset_ = originalDataset;

    return scalabilityResults;
}

std::pair<double, double> PerformanceTester::calculateStatistics(const std::vector<double>& values) const {
    if (values.empty()) {
        return std::make_pair(0.0, 0.0);
    }

    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();

    double variance = 0.0;
    for (double value : values) {
        variance += (value - mean) * (value - mean);
    }
    variance /= values.size();

    return std::make_pair(mean, std::sqrt(variance));
}

std::pair<double, double> PerformanceTester::calculateTimeStatistics(const std::vector<long long>& values) const {
    if (values.empty()) {
        return std::make_pair(0.0, 0.0);
    }

    double mean = static_cast<double>(std::accumulate(values.begin(), values.end(), 0LL)) / values.size();

    double variance = 0.0;
    for (long long value : values) {
        double diff = static_cast<double>(value) - mean;
        variance += diff * diff;
    }
    variance /= values.size();

    return std::make_pair(mean, std::sqrt(variance));
}

void PerformanceTester::createDirectoryIfNotExists(const std::string& path) const {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        // Создаем директорию
        mkdir(path.c_str(), 0755);
        // Для простоты игнорируем ошибки - в реальном коде нужна более тщательная обработка
    }
}

std::vector<std::string> PerformanceTester::getFilesInDirectory(const std::string& directory) const {
    std::vector<std::string> files;

    DIR* dir = opendir(directory.c_str());
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".csv") {
                files.push_back(filename);
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Error reading directory " << directory << std::endl;
    }

    return files;
}