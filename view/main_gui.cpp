#include "signal_visualizer.h"
#include "../src/signal_generator.h"
#include "../src/median_filter.h"
#include "../src/wiener_filter.h"
#include "../src/robust_wiener_filter.h"
#include "../src/morphological_filter.h"
#include "../src/outlier_detection.h"
#include "../src/savgol_filter.h"
#include "../src/kalman_filter.h"
#include "../src/spectral_subtraction_filter.h"
#include "../src/signal_classifier.h"
#include "../src/adaptive_filter_selector.h"
#include "../src/doppler_nip_filter.h"
#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <format>
#include <iomanip>

// ─── Режим РЛС: обработка НИП доплеровскими фазовыми фильтрами ───────────────
/**
 * Запустить режим визуализации РЛС (split-view).
 *
 * @param noisyFile   CSV-файл с НИП (Re,Im на строку)
 * @param cleanFile   CSV-файл чистого сигнала (Re,Im, опционально)
 * @param threshold   Порог CV обнаружения НИП (по умолчанию 0.5)
 * @return            0 при успехе
 */
int runRadarMode(const std::string& noisyFile,
                 const std::string& cleanFile,
                 double threshold = 0.50)
{
    std::cout << "\n=== РЕЖИМ РЛС: ПОДАВЛЕНИЕ НИП (ДОПЛЕРОВСКИЕ ФАЗОВЫЕ ФИЛЬТРЫ) ===\n\n";

    // ── Загрузка входных данных ────────────────────────────────────────────
    std::cout << "Загрузка зашумлённой пачки: " << noisyFile << "\n";
    ComplexSignal noisyBurst = DopplerNipFilter::loadFromCSV(noisyFile);
    if (noisyBurst.empty()) {
        std::cerr << "Ошибка: файл пуст или не найден: " << noisyFile << "\n";
        return 1;
    }
    std::cout << "  N = " << noisyBurst.size() << " импульсов\n";

    ComplexSignal cleanBurst;
    if (!cleanFile.empty()) {
        std::cout << "Загрузка чистой пачки: " << cleanFile << "\n";
        cleanBurst = DopplerNipFilter::loadFromCSV(cleanFile);
    }

    // ── Применяем алгоритм ────────────────────────────────────────────────
    DopplerNipFilter nipFilter(threshold, 0.05, 0);
    ComplexSignal filteredBurst = nipFilter.process(noisyBurst);

    const NipDetectionResult& det = nipFilter.getLastDetection();
    std::cout << "\n--- Результат обнаружения ---\n";
    if (det.detected) {
        std::cout << "  НИП обнаружена!\n";
        std::cout << "  Импульс m = " << det.pulseIndex << "\n";
        std::cout << "  Амплитуда = " << std::fixed << std::setprecision(4) << det.amplitude << "\n";
        std::cout << "  Фаза φ₀  = " << det.phaseRad << " рад\n";
        std::cout << "  CV-метрика= " << det.detectionMetric << "\n";
    } else {
        std::cout << "  НИП не обнаружена (CV = " << det.detectionMetric << ")\n";
    }

    // ── Временные сигналы: огибающие |x[n]| ──────────────────────────────
    SignalProcessor::Signal noisyMag    = DopplerNipFilter::getMagnitude(noisyBurst);
    SignalProcessor::Signal filteredMag = DopplerNipFilter::getMagnitude(filteredBurst);
    SignalProcessor::Signal cleanMag;
    if (!cleanBurst.empty())
        cleanMag = DopplerNipFilter::getMagnitude(cleanBurst);

    // ── Спектральные данные ───────────────────────────────────────────────
    SignalProcessor::Signal specBefore = nipFilter.getSpectrumBefore();
    SignalProcessor::Signal specAfter  = nipFilter.getSpectrumAfter();
    SignalProcessor::Signal specDiff   = nipFilter.getSpectrumDiff();

    // ── OpenGL визуализация ───────────────────────────────────────────────
    std::cout << "\nЗапуск OpenGL split-view...\n";
    std::cout << "  Верхняя панель: огибающая |x[n]| (время / импульсы)\n";
    std::cout << "  Нижняя панель: доплеровский спектр (дБ)\n";
    std::cout << "  Клавиши: G=чистый N=зашумлённый F=компенсированный\n";
    std::cout << "           1=спектр_до 2=спектр_после 3=разность_НИП\n";

    std::string title = "Doppler NIP Filter | N=" + std::to_string(noisyBurst.size());
    if (det.detected)
        title += " | НИП обнаружена [m=" + std::to_string(det.pulseIndex) + "]";
    else
        title += " | НИП не обнаружена";

    SignalVisualizer visualizer(1400, 850, title);
    if (!visualizer.initialize()) {
        std::cerr << "Ошибка инициализации OpenGL\n";
        return 1;
    }

    // Устанавливаем временной сигнал (огибающие)
    visualizer.setSignalData(noisyMag, filteredMag, cleanMag);

    // Включаем split-view со спектрами
    visualizer.enableSplitView(specBefore, specAfter, specDiff, 0.60f);

    visualizer.run();
    return 0;
}

void printUsage(const char* programName) {
    std::cout << "Использование: " << programName << " [опции]\n\n";
    std::cout << "Опции:\n";
    std::cout << "  --radar FILE             РЛС-режим: подавление НИП (split-view)\n";
    std::cout << "                           FILE — CSV с НИП (Re,Im на строку)\n";
    std::cout << "  --radar-clean FILE       Чистая пачка для сравнения (Re,Im CSV)\n";
    std::cout << "  --nip-threshold THR      Порог CV для обнаружения НИП (по умолч. 0.50)\n";
    std::cout << "  -f, --filter TYPE        Тип фильтра: median, wiener, robust_wiener, robust_wiener_auto, morpho, outlier, savgol, kalman, spectral, auto\n";
    std::cout << "  -i, --input FILE         Входной файл с зашумленным сигналом (.csv)\n";
    std::cout << "  -c, --clean FILE         Чистый сигнал для сравнения (.csv)\n";
    std::cout << "  -p, --params PARAMS      Параметры фильтра (зависят от типа)\n";
    std::cout << "  --prefilter              Предварительная обработка outlier_detection (MAD,linear,3.0,11)\n";
    std::cout << "  -h, --help               Показать эту справку\n\n";

    std::cout << "Параметры фильтров:\n";
    std::cout << "  median:                  window_size (нечетное число, по умолчанию 7)\n";
    std::cout << "  wiener:                  order,desired_window,regularization (по умолчанию 8,5,1e-4)\n";
    std::cout << "  robust_wiener:           order,desired_window,regularization,outlier_threshold,outlier_window\n";
    std::cout << "                           (по умолчанию 10,5,1e-4,3.5,11)\n";
    std::cout << "                           Улучшения: медианная оценка d[n], детекция импульсов, zero-padding\n";
    std::cout << "  robust_wiener_auto:      параметры подбираются автоматически по входному сигналу\n";
    std::cout << "                           A: sigma_noise=MAD/0.6745 → outlierThreshold, regularization=σ²\n";
    std::cout << "                           B: FFT → f_95 → filterOrder=round(1/(2·f_95))\n";
    std::cout << "                           Подходит когда параметры сигнала заранее неизвестны\n";
    std::cout << "  morpho:                  operation,size (operation: opening/closing, по умолчанию opening,5)\n";
    std::cout << "  outlier:                 method,interpolation,threshold,window (по умолчанию mad,linear,3.0,11)\n";
    std::cout << "  savgol:                  window_size,poly_order (по умолчанию 11,3)\n";
    std::cout << "  kalman:                  process_noise,measurement_noise,delta_t (по умолчанию 0.1,1.0,1.0)\n";
    std::cout << "  spectral:                frame_size,hop_size,noise_frames,alpha,floor,mu,gamma\n";
    std::cout << "                           (по умолчанию 256,64,4,2.0,0.002,0.1,1.5)\n";
    std::cout << "                           FFT-based спектральное вычитание (Boll, 1979)\n";
    std::cout << "  auto:                    автоматический выбор фильтра по типу сигнала\n";
    std::cout << "                           Классификация: SINE→Wiener, SQUARE→Median, TRIANGLE→SavGol,\n";
    std::cout << "                           ECHO→Kalman, NOISY→RobustWiener, UNKNOWN→Median\n\n";

    std::cout << "Примеры:\n";
    std::cout << "  " << programName << " -f median         -i data/noisy/signal_1.csv -c data/clean/signal_1.csv\n";
    std::cout << "  " << programName << " -f wiener         -i data/noisy/signal_1.csv -c data/clean/signal_1.csv -p 32,31,1e-4\n";
    std::cout << "  " << programName << " -f robust_wiener  -i data/noisy/signal_1.csv -c data/clean/signal_1.csv\n";
    std::cout << "  " << programName << " -f robust_wiener  -i data/noisy/signal_1.csv -c data/clean/signal_1.csv -p 10,5,1e-4,3.5,11\n";
    std::cout << "  " << programName << " -f robust_wiener_auto -i data/wiener/noisy/signal_0.csv -c data/wiener/clean/signal_0.csv\n";
    std::cout << "  " << programName << " -f kalman         -i data/noisy/signal_1.csv -c data/clean/signal_1.csv --prefilter\n";
    std::cout << "  " << programName << " -f spectral       -i data/noisy/signal_1.csv -c data/clean/signal_1.csv\n";
    std::cout << "  " << programName << " -f spectral       -i data/noisy/signal_1.csv -c data/clean/signal_1.csv -p 512,128,6,3.0,0.002\n";
    std::cout << "  " << programName << " -f auto           -i data/noisy/signal_1.csv -c data/clean/signal_1.csv\n";
}

struct FilterParams {
    std::string filterType;
    std::string inputFile;
    std::string cleanFile;
    std::string params;
    bool        prefilter    = false; ///< запустить outlier_detection перед основным фильтром
    // РЛС-режим
    bool        radarMode    = false;
    std::string radarFile;            ///< файл пачки с НИП (Re,Im)
    std::string radarCleanFile;       ///< чистая пачка (Re,Im)
    double      nipThreshold = 0.50;  ///< порог CV для обнаружения НИП
};

FilterParams parseCommandLine(int argc, char* argv[]) {
    FilterParams params;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            exit(0);
        }
        // ── РЛС-режим ──────────────────────────────────────────────────────
        else if (arg == "--radar" && i + 1 < argc) {
            params.radarMode = true;
            params.radarFile = argv[++i];
        }
        else if (arg == "--radar-clean" && i + 1 < argc) {
            params.radarCleanFile = argv[++i];
        }
        else if (arg == "--nip-threshold" && i + 1 < argc) {
            params.nipThreshold = std::stod(argv[++i]);
        }
        // ── Обычный режим ──────────────────────────────────────────────────
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
        else if (arg == "--prefilter") {
            params.prefilter = true;
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
        int    order          = 8;
        int    desiredWindow  = 5;
        double regularization = 1e-4;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) order         = std::stoi(parts[0]);
            if (parts.size() >= 2) desiredWindow = std::stoi(parts[1]);
            if (parts.size() >= 3) regularization = std::stod(parts[2]);
        }
        return std::make_unique<WienerFilter>(
            static_cast<size_t>(order),
            static_cast<size_t>(desiredWindow),
            regularization);
    }
    else if (type == "robust_wiener") {
        int    order            = 10;
        int    desiredWindow    = 5;
        double regularization   = 1e-4;
        double outlierThreshold = 3.5;
        int    outlierWindow    = 11;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) order            = std::stoi(parts[0]);
            if (parts.size() >= 2) desiredWindow    = std::stoi(parts[1]);
            if (parts.size() >= 3) regularization   = std::stod(parts[2]);
            if (parts.size() >= 4) outlierThreshold = std::stod(parts[3]);
            if (parts.size() >= 5) outlierWindow    = std::stoi(parts[4]);
        }
        return std::make_unique<RobustWienerFilter>(
            static_cast<size_t>(order),
            static_cast<size_t>(desiredWindow),
            regularization,
            outlierThreshold,
            static_cast<size_t>(outlierWindow));
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
        OutlierDetection::DetectionMethod    method = OutlierDetection::DetectionMethod::MAD_BASED;
        OutlierDetection::InterpolationMethod interp = OutlierDetection::InterpolationMethod::LINEAR;
        double threshold = 3.0;
        int    window    = 11;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) {
                if (parts[0] == "statistical") method = OutlierDetection::DetectionMethod::STATISTICAL;
                else if (parts[0] == "adaptive") method = OutlierDetection::DetectionMethod::ADAPTIVE_THRESHOLD;
            }
            if (parts.size() >= 2) {
                if (parts[1] == "median")        interp = OutlierDetection::InterpolationMethod::MEDIAN_BASED;
                else if (parts[1] == "autoregressive") interp = OutlierDetection::InterpolationMethod::AUTOREGRESSIVE;
            }
            if (parts.size() >= 3) threshold = std::stod(parts[2]);
            if (parts.size() >= 4) window    = std::stoi(parts[3]);
        }
        return std::make_unique<OutlierDetection>(method, interp, threshold, window);
    }
    else if (type == "savgol") {
        int windowSize = 11;
        int polyOrder  = 3;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) windowSize = std::stoi(parts[0]);
            if (parts.size() >= 2) polyOrder  = std::stoi(parts[1]);
        }
        return std::make_unique<SavgolFilter>(windowSize, polyOrder);
    }
    else if (type == "kalman") {
        double processNoise      = 0.1;
        double measurementNoise  = 1.0;
        double deltaT            = 1.0;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) processNoise     = std::stod(parts[0]);
            if (parts.size() >= 2) measurementNoise = std::stod(parts[1]);
            if (parts.size() >= 3) deltaT           = std::stod(parts[2]);
        }
        return std::make_unique<KalmanFilter>(processNoise, measurementNoise, deltaT);
    }
    else if (type == "spectral") {
        // frame_size, hop_size, noise_frames, alpha, floor, mu, gamma
        int    frameSize         = 256;
        int    hopSize           = 64;
        int    noiseFrames       = 4;
        double subtractionFactor = 2.0;
        double spectralFloor     = 0.002;
        double noiseUpdateRate   = 0.1;
        double noiseThreshold    = 1.5;

        if (!params.empty()) {
            auto parts = split(params, ',');
            if (parts.size() >= 1) frameSize         = std::stoi(parts[0]);
            if (parts.size() >= 2) hopSize           = std::stoi(parts[1]);
            if (parts.size() >= 3) noiseFrames       = std::stoi(parts[2]);
            if (parts.size() >= 4) subtractionFactor = std::stod(parts[3]);
            if (parts.size() >= 5) spectralFloor     = std::stod(parts[4]);
            if (parts.size() >= 6) noiseUpdateRate   = std::stod(parts[5]);
            if (parts.size() >= 7) noiseThreshold    = std::stod(parts[6]);
        }
        return std::make_unique<SpectralSubtractionFilter>(
            static_cast<size_t>(frameSize),
            static_cast<size_t>(hopSize),
            static_cast<size_t>(noiseFrames),
            subtractionFactor,
            spectralFloor,
            noiseUpdateRate,
            noiseThreshold);
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

        // ── РЛС-режим (--radar) ───────────────────────────────────────────
        if (params.radarMode) {
            return runRadarMode(params.radarFile,
                                params.radarCleanFile,
                                params.nipThreshold);
        }

        if (params.filterType.empty()) {
            std::cerr << "Ошибка: необходимо указать тип фильтра (-f)" << std::endl;
            return 1;
        }

        if (params.inputFile.empty()) {
            std::cerr << "Ошибка: необходимо указать входной файл (-i)" << std::endl;
            return 1;
        }

        // ── Загрузка сигналов ─────────────────────────────────────────────
        std::cout << "Загрузка зашумленного сигнала: " << params.inputFile << "\n";
        auto noisySignal = SignalGenerator::loadSignalFromCSV(params.inputFile);

        SignalProcessor::Signal cleanSignal;
        if (!params.cleanFile.empty()) {
            std::cout << "Загрузка чистого сигнала: " << params.cleanFile << "\n";
            cleanSignal = SignalGenerator::loadSignalFromCSV(params.cleanFile);
        }

        // ── Предфильтрация outlier_detection ─────────────────────────────
        SignalProcessor::Signal inputForFilter = noisySignal;
        long long prefilterTime = 0;

        if (params.prefilter) {
            std::cout << "Предфильтрация: OutlierDetection(MAD, linear, 3.0, 11)...\n";
            OutlierDetection prefilter(
                OutlierDetection::DetectionMethod::MAD_BASED,
                OutlierDetection::InterpolationMethod::LINEAR,
                3.0, 11);
            auto [preOut, preTime] = prefilter.measurePerformance(noisySignal);
            inputForFilter = preOut;
            prefilterTime  = preTime;
            std::cout << "  Предфильтрация завершена за " << prefilterTime << " мкс\n";
        }

        // ── Основной фильтр ───────────────────────────────────────────────
        std::cout << "Создание фильтра: " << params.filterType;
        if (!params.params.empty()) std::cout << " (параметры: " << params.params << ")";
        std::cout << "\n";

        SignalProcessor::Signal filteredSignal;
        long long filterTime = 0;
        std::string algorithmDescription;

        if (params.filterType == "robust_wiener_auto") {
            // ── Робастный Винер с автоподбором параметров A+B ─────────────────
            std::cout << "Анализ входного сигнала для авто-настройки параметров...\n";

            WienerParams wp = RobustWienerFilter::estimateParameters(inputForFilter);

            std::cout << "\n=== АВТО-НАСТРОЙКА RobustWienerFilter ===\n";
            std::cout << "  Оценка σ_noise (MAD/0.6745): " << std::fixed << std::setprecision(4)
                      << wp.estimatedNoiseSigma << "\n";
            std::cout << "  RMS сигнала:                 " << wp.estimatedSignalRMS << "\n";
            std::cout << "  Оценка SNR:                  " << std::setprecision(1)
                      << wp.estimatedSNR_dB << " дБ\n";
            std::cout << "  Доминирующая частота:        " << std::setprecision(4)
                      << wp.dominantFrequency << " (нормированная)\n";
            std::cout << "\nПодобранные параметры:\n";
            std::cout << "  filterOrder      = " << wp.filterOrder      << "\n";
            std::cout << "  desiredWindow    = " << wp.desiredWindow    << "\n";
            std::cout << "  regularization   = " << std::scientific << wp.regularization   << "\n";
            std::cout << "  outlierThreshold = " << std::fixed << std::setprecision(2)
                      << wp.outlierThreshold << "\n";
            std::cout << "  outlierWindow    = " << wp.outlierWindow    << "\n\n";

            RobustWienerFilter filter(wp.filterOrder, wp.desiredWindow,
                                      wp.regularization, wp.outlierThreshold,
                                      wp.outlierWindow);

            auto t0 = std::chrono::high_resolution_clock::now();
            filteredSignal = filter.process(inputForFilter);
            auto t1 = std::chrono::high_resolution_clock::now();
            filterTime = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

            algorithmDescription = "RobustWienerAuto[ord=" + std::to_string(wp.filterOrder)
                + ",win=" + std::to_string(wp.desiredWindow)
                + ",thr=" + std::to_string(static_cast<int>(wp.outlierThreshold * 10) / 10)
                + "]";

        } else if (params.filterType == "auto") {
            // ── Автоматический режим: классификация → выбор → применение ─────
            AdaptiveFilterSelector selector;
            SignalClassifier::SignalType detectedType;
            std::string chosenFilterName;

            auto t0 = std::chrono::high_resolution_clock::now();
            filteredSignal = selector.processAuto(inputForFilter, detectedType, chosenFilterName);
            auto t1 = std::chrono::high_resolution_clock::now();
            filterTime = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

            std::cout << "\n=== АВТОМАТИЧЕСКАЯ КЛАССИФИКАЦИЯ ===\n";
            std::cout << "Обнаруженный тип сигнала: "
                      << SignalClassifier::typeToString(detectedType) << "\n";
            std::cout << "Причина выбора: "
                      << AdaptiveFilterSelector::getSelectionReason(detectedType) << "\n";
            std::cout << "Выбранный фильтр: " << chosenFilterName << "\n";

            algorithmDescription = "AUTO[" + SignalClassifier::typeToString(detectedType)
                                   + "] → " + chosenFilterName;
        } else {
            auto filter = createFilter(params.filterType, params.params);
            auto [out, ft] = filter->measurePerformance(inputForFilter);
            filteredSignal = std::move(out);
            filterTime     = ft;
            if (params.prefilter)
                algorithmDescription = "OutlierDetection → " + filter->getName();
            else
                algorithmDescription = filter->getName();
        }

        // ── Метрики ───────────────────────────────────────────────────────
        std::cout << "\n=== РЕЗУЛЬТАТЫ ФИЛЬТРАЦИИ ===\n";
        std::cout << "Алгоритм: " << algorithmDescription << "\n";

        std::cout << "Время предфильтра: " << prefilterTime << " мкс\n";
        std::cout << "Время основного фильтра: " << filterTime  << " мкс\n";
        std::cout << "Суммарное время: " << (prefilterTime + filterTime) << " мкс\n";

        if (!cleanSignal.empty()) {
            // Метрики после предфильтра (если включён)
            if (params.prefilter) {
                double snrPre  = calculateSNR(cleanSignal, inputForFilter);
                double msePre  = calculateMSE(cleanSignal, inputForFilter);
                std::cout << "\nПосле предфильтра:\n";
                std::cout << "  SNR: " << std::fixed << std::setprecision(2) << snrPre << " дБ\n";
                std::cout << "  MSE: " << std::scientific << std::setprecision(2) << msePre << "\n";
            }

            double snr = calculateSNR(cleanSignal, filteredSignal);
            double mse = calculateMSE(cleanSignal, filteredSignal);
            double correlation = calculateCorrelation(cleanSignal, filteredSignal);

            std::cout << "\nПосле основного фильтра:\n";
            std::cout << "  SNR: " << std::fixed << std::setprecision(2) << snr << " дБ\n";
            std::cout << "  MSE: " << std::scientific << std::setprecision(2) << mse << "\n";
            std::cout << "  Корреляция: " << std::fixed << std::setprecision(3) << correlation << "\n";
        } else {
            std::cout << "Метрики качества не рассчитаны (отсутствует чистый сигнал)\n";
        }

        // ── OpenGL визуализация ───────────────────────────────────────────
        std::cout << "\nИнициализация OpenGL визуализации...\n";

        std::string title = "Signal Filter Visualization - ";
        title += algorithmDescription;

        SignalVisualizer visualizer(1200, 800, title);

        if (!visualizer.initialize()) {
            std::cerr << "Ошибка инициализации визуализатора\n";
            return 1;
        }

        // Передаём: noisy = исходный зашумлённый, filtered = результат цепочки, clean = эталон
        visualizer.setSignalData(noisySignal, filteredSignal, cleanSignal);

        std::cout << "\nЛегенда цветов:\n";
        if (!cleanSignal.empty()) std::cout << "  Зеленый - чистый сигнал\n";
        std::cout << "  Красный - зашумленный сигнал\n";
        std::cout << "  Синий   - отфильтрованный сигнал";
        if (params.filterType == "auto" || params.prefilter)
            std::cout << " (" << algorithmDescription << ")";
        std::cout << "\n";

        visualizer.run();

    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Программа завершена.\n";
    return 0;
}
