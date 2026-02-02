#include "signal_visualizer.h"
#include "../src/signal_generator.h"
#include "../src/median_filter.h"
#include "../src/wiener_filter.h"
#include "../src/morphological_filter.h"
#include "../src/outlier_detection.h"
#include "../src/savgol_filter.h"
#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <format>
#include <iomanip>

void printUsage(const char* programName) {
    std::cout << "Использование: " << programName << " [опции]\n\n";
    std::cout << "Опции:\n";
    std::cout << "  -f, --filter TYPE        Тип фильтра: median, wiener, morpho, outlier, savgol\n";
    std::cout << "  -i, --input FILE         Входной файл с зашумленным сигналом (.csv)\n";
    std::cout << "  -c, --clean FILE         Чистый сигнал для сравнения (.csv)\n";
    std::cout << "  -p, --params PARAMS      Параметры фильтра (зависят от типа)\n";
    std::cout << "  -h, --help               Показать эту справку\n\n";

    std::cout << "Параметры фильтров:\n";
    std::cout << "  median:                  window_size (нечетное число, по умолчанию 7)\n";
    std::cout << "  wiener:                  order,regularization,adaptation (по умолчанию 8,0.01,0.99)\n";
    std::cout << "  morpho:                  operation,size (operation: opening/closing, по умолчанию opening,5)\n";
    std::cout << "  outlier:                 method,interpolation,threshold,window (по умолчанию mad,linear,3.0,11)\n";
    std::cout << "  savgol:                  window_size,poly_order (по умолчанию 11,3)\n\n";

    std::cout << "Примеры:\n";
    std::cout << "  " << programName << " -f median -i data/noisy/Sine_noisy.csv -c data/clean/Sine_clean.csv\n";
    std::cout << "  " << programName << " -f median -i data/noisy/Sine_noisy.csv -p 9\n";
    std::cout << "  " << programName << " -f wiener -i data/noisy/Triangle_noisy.csv -p 10,0.005,0.995\n";
}

struct FilterParams {
    std::string filterType;
    std::string inputFile;
    std::string cleanFile;
    std::string params;
};

FilterParams parseCommandLine(int argc, char* argv[]) {
    FilterParams params;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            exit(0);
        }
        else if ((arg == "-f" || arg == "--filter") && i + 1 < argc) {
            params.filterType = argv[++i];
        }
        else if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            params.inputFile = argv[++i];
        }
        else if ((arg == "-c" || arg == "--clean") && i + 1 < argc) {
            params.cleanFile = argv[++i];
        }
        else if ((arg == "-p" || arg == "--params") && i + 1 < argc) {
            params.params = argv[++i];
        }
        else {
            std::cerr << "Неизвестный параметр: " << arg << std::endl;
            printUsage(argv[0]);
            exit(1);
        }
    }

    return params;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::string current;

    for (char c : str) {
        if (c == delimiter) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

std::unique_ptr<SignalProcessor> createFilter(const std::string& type, const std::string& params) {
    if (type == "median") {
        int windowSize = 7;
        if (!params.empty()) {
            windowSize = std::stoi(params);
        }
        return std::make_unique<MedianFilter>(windowSize);
    }
    else if (type == "wiener") {
        int order = 8;
        double regularization = 0.01;
        double adaptation = 0.99;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) order = std::stoi(parts[0]);
            if (parts.size() >= 2) regularization = std::stod(parts[1]);
            if (parts.size() >= 3) adaptation = std::stod(parts[2]);
        }
        return std::make_unique<WienerFilter>(order, regularization, adaptation);
    }
    else if (type == "morpho") {
        MorphologicalFilter::Operation op = MorphologicalFilter::Operation::OPENING;
        int size = 5;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) {
                if (parts[0] == "closing") {
                    op = MorphologicalFilter::Operation::CLOSING;
                }
            }
            if (parts.size() >= 2) size = std::stoi(parts[1]);
        }
        return std::make_unique<MorphologicalFilter>(op, size);
    }
    else if (type == "outlier") {
        OutlierDetection::DetectionMethod method = OutlierDetection::DetectionMethod::MAD_BASED;
        OutlierDetection::InterpolationMethod interp = OutlierDetection::InterpolationMethod::LINEAR;
        double threshold = 3.0;
        int window = 11;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) {
                if (parts[0] == "statistical") method = OutlierDetection::DetectionMethod::STATISTICAL;
                else if (parts[0] == "adaptive") method = OutlierDetection::DetectionMethod::ADAPTIVE_THRESHOLD;
            }
            if (parts.size() >= 2) {
                if (parts[1] == "median") interp = OutlierDetection::InterpolationMethod::MEDIAN_BASED;
                else if (parts[1] == "autoregressive") interp = OutlierDetection::InterpolationMethod::AUTOREGRESSIVE;
            }
            if (parts.size() >= 3) threshold = std::stod(parts[2]);
            if (parts.size() >= 4) window = std::stoi(parts[3]);
        }
        return std::make_unique<OutlierDetection>(method, interp, threshold, window);
    }
    else if (type == "savgol") {
        int windowSize = 11;
        int polyOrder = 3;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) windowSize = std::stoi(parts[0]);
            if (parts.size() >= 2) polyOrder = std::stoi(parts[1]);
        }
        return std::make_unique<SavgolFilter>(windowSize, polyOrder);
    }
    else {
        throw std::runtime_error("Неизвестный тип фильтра: " + type);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "================================================\n";
    std::cout << "  ВИЗУАЛИЗАЦИЯ ФИЛЬТРАЦИИ РАДИОЛОКАЦИОННЫХ СИГНАЛОВ\n";
    std::cout << "================================================\n\n";

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        auto params = parseCommandLine(argc, argv);

        // Проверка обязательных параметров
        if (params.filterType.empty()) {
            std::cerr << "Ошибка: необходимо указать тип фильтра (-f)" << std::endl;
            return 1;
        }

        if (params.inputFile.empty()) {
            std::cerr << "Ошибка: необходимо указать входной файл (-i)" << std::endl;
            return 1;
        }

        // Загрузка сигналов
        std::cout << "Загрузка зашумленного сигнала: " << params.inputFile << std::endl;
        auto noisySignal = SignalGenerator::loadSignalFromCSV(params.inputFile);

        SignalProcessor::Signal cleanSignal;
        if (!params.cleanFile.empty()) {
            std::cout << "Загрузка чистого сигнала: " << params.cleanFile << std::endl;
            cleanSignal = SignalGenerator::loadSignalFromCSV(params.cleanFile);
        }

        // Создание фильтра
        std::cout << "Создание фильтра: " << params.filterType;
        if (!params.params.empty()) {
            std::cout << " (параметры: " << params.params << ")";
        }
        std::cout << std::endl;

        auto filter = createFilter(params.filterType, params.params);

        // Применение фильтрации
        std::cout << "Применение фильтрации..." << std::endl;
        auto [filteredSignal, executionTime] = filter->measurePerformance(noisySignal);

        // Вычисление и вывод метрик
        std::cout << "\n=== РЕЗУЛЬТАТЫ ФИЛЬТРАЦИИ ===\n";
        std::cout << "Алгоритм: " << filter->getName() << std::endl;
        std::cout << "Время выполнения: " << executionTime << " мкс" << std::endl;

        if (!cleanSignal.empty()) {
            double snr = calculateSNR(cleanSignal, filteredSignal);
            double mse = calculateMSE(cleanSignal, filteredSignal);
            double correlation = calculateCorrelation(cleanSignal, filteredSignal);

            std::cout << "SNR: " << std::fixed << std::setprecision(2) << snr << " дБ" << std::endl;
            std::cout << "MSE: " << std::scientific << std::setprecision(2) << mse << std::endl;
            std::cout << "Корреляция: " << std::fixed << std::setprecision(3) << correlation << std::endl;
        } else {
            std::cout << "Метрики качества не рассчитаны (отсутствует чистый сигнал)" << std::endl;
        }

        // Инициализация и запуск визуализации
        std::cout << "\nИнициализация OpenGL визуализации..." << std::endl;

        SignalVisualizer visualizer(1200, 800, "Signal Filter Visualization - " + filter->getName());

        if (!visualizer.initialize()) {
            std::cerr << "Ошибка инициализации визуализатора" << std::endl;
            return 1;
        }

        // Установка данных для визуализации
        visualizer.setSignalData(noisySignal, filteredSignal, cleanSignal);

        std::cout << "\nЛегенда цветов:" << std::endl;
        if (!cleanSignal.empty()) {
            std::cout << "  Зеленый - чистый сигнал" << std::endl;
        }
        std::cout << "  Красный - зашумленный сигнал" << std::endl;
        std::cout << "  Синий - отфильтрованный сигнал" << std::endl;

        // Запуск визуализации
        visualizer.run();

    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Программа завершена." << std::endl;
    return 0;
}