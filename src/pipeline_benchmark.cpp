/**
 * pipeline_benchmark — сравнение одиночных фильтров vs двухэтапных цепочек
 * outlier_detection → <filter>
 *
 * Запуск:
 *   ./build/pipeline_benchmark [signal_N.csv]
 *
 * По умолчанию перебирает все signal_0..signal_9.csv и выводит сводную таблицу.
 */

#include <iostream>
#include <iomanip>
#include <format>
#include <vector>
#include <string>
#include <memory>
#include <numeric>
#include <cmath>

#include "signal_generator.h"
#include "signal_processor.h"
#include "outlier_detection.h"
#include "median_filter.h"
#include "wiener_filter.h"
#include "morphological_filter.h"
#include "savgol_filter.h"
#include "kalman_filter.h"

// ─────────────────────────────────────────────────────────────────────────────
// Результат одного запуска фильтра
// ─────────────────────────────────────────────────────────────────────────────
struct RunResult {
    std::string label;       ///< Имя конфигурации
    double      snr;
    double      mse;
    double      correlation;
    long long   timeUs;      ///< Суммарное время (мкс)
};

// ─────────────────────────────────────────────────────────────────────────────
// Стандартный преdfильтр: OutlierDetection(MAD, linear, 3.0, 11)
// ─────────────────────────────────────────────────────────────────────────────
static SignalProcessor::Signal applyPrefilter(
    const SignalProcessor::Signal& noisy,
    long long& outTimeUs)
{
    OutlierDetection pre(
        OutlierDetection::DetectionMethod::MAD_BASED,
        OutlierDetection::InterpolationMethod::LINEAR,
        3.0, 11);
    auto [out, t] = pre.measurePerformance(noisy);
    outTimeUs = t;
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Запуск одиночного фильтра
// ─────────────────────────────────────────────────────────────────────────────
static RunResult runSingle(
    const std::string&             label,
    SignalProcessor&               filter,
    const SignalProcessor::Signal& noisy,
    const SignalProcessor::Signal& clean)
{
    auto [filtered, t] = filter.measurePerformance(noisy);
    return RunResult{
        label,
        calculateSNR(clean, filtered),
        calculateMSE(clean, filtered),
        calculateCorrelation(clean, filtered),
        t
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Запуск двухэтапной цепочки: outlier → filter
// ─────────────────────────────────────────────────────────────────────────────
static RunResult runPipeline(
    const std::string&             label,
    SignalProcessor&               filter,
    const SignalProcessor::Signal& noisy,
    const SignalProcessor::Signal& clean)
{
    long long preTime = 0;
    auto preOut = applyPrefilter(noisy, preTime);

    auto [filtered, filterTime] = filter.measurePerformance(preOut);

    return RunResult{
        "Outlier→" + label,
        calculateSNR(clean, filtered),
        calculateMSE(clean, filtered),
        calculateCorrelation(clean, filtered),
        preTime + filterTime
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Напечатать строку таблицы
// ─────────────────────────────────────────────────────────────────────────────
static void printRow(const RunResult& r) {
    std::cout << std::format("{:<38} {:>8.2f} {:>14.3e} {:>12.4f} {:>10}\n",
        r.label, r.snr, r.mse, r.correlation, r.timeUs);
}

// ─────────────────────────────────────────────────────────────────────────────
// Усреднить результаты по нескольким сигналам
// ─────────────────────────────────────────────────────────────────────────────
static RunResult average(const std::vector<RunResult>& v) {
    RunResult avg = v[0];
    for (size_t i = 1; i < v.size(); ++i) {
        avg.snr         += v[i].snr;
        avg.mse         += v[i].mse;
        avg.correlation += v[i].correlation;
        avg.timeUs      += v[i].timeUs;
    }
    double n = static_cast<double>(v.size());
    avg.snr         /= n;
    avg.mse         /= n;
    avg.correlation /= n;
    avg.timeUs      = static_cast<long long>(avg.timeUs / n);
    avg.label       = "[avg] " + avg.label;
    return avg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Конфигурации фильтров для тестирования
// ─────────────────────────────────────────────────────────────────────────────
struct FilterConfig {
    std::string                          name;
    std::function<std::unique_ptr<SignalProcessor>()> factory;
};

static std::vector<FilterConfig> makeConfigs() {
    return {
        { "Median(7)",
          [] { return std::make_unique<MedianFilter>(7); } },

        { "Median(15)",
          [] { return std::make_unique<MedianFilter>(15); } },

        { "Wiener(8,5)",
          [] { return std::make_unique<WienerFilter>(8, 5, 1e-4); } },

        { "Wiener(32,31)",
          [] { return std::make_unique<WienerFilter>(32, 31, 1e-4); } },

        { "Morpho(opening,5)",
          [] { return std::make_unique<MorphologicalFilter>(
                   MorphologicalFilter::Operation::OPENING, 5); } },

        { "Morpho(closing,5)",
          [] { return std::make_unique<MorphologicalFilter>(
                   MorphologicalFilter::Operation::CLOSING, 5); } },

        { "SavGol(11,3)",
          [] { return std::make_unique<SavgolFilter>(11, 3); } },

        { "SavGol(21,4)",
          [] { return std::make_unique<SavgolFilter>(21, 4); } },

        { "Kalman(0.1,1.0)",
          [] { return std::make_unique<KalmanFilter>(0.1, 1.0, 1.0); } },

        { "Kalman(0.01,1.0)",
          [] { return std::make_unique<KalmanFilter>(0.01, 1.0, 1.0); } },

        { "OutlierOnly(MAD,linear)",
          [] { return std::make_unique<OutlierDetection>(
                   OutlierDetection::DetectionMethod::MAD_BASED,
                   OutlierDetection::InterpolationMethod::LINEAR,
                   3.0, 11); } },
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    std::cout << "================================================\n";
    std::cout << "  PIPELINE BENCHMARK: одиночные vs outlier→filter\n";
    std::cout << "================================================\n\n";

    // Собираем список сигналов
    std::vector<std::string> signalFiles;
    if (argc >= 2) {
        signalFiles.emplace_back(argv[1]);
    } else {
        for (int n = 0; n <= 9; ++n)
            signalFiles.push_back(std::format("signal_{}.csv", n));
    }

    std::string rootPath(ROOT_PATH);
    auto configs = makeConfigs();

    // Для каждой конфигурации храним результаты по всем сигналам
    // results[cfg_idx][single/pipeline] → vector<RunResult>
    const size_t C = configs.size();
    std::vector<std::vector<RunResult>> singleResults(C);
    std::vector<std::vector<RunResult>> pipeResults(C);

    // ── Перебираем сигналы ────────────────────────────────────────────────
    for (const auto& fname : signalFiles) {
        std::string cleanPath = rootPath + "/data/clean/"  + fname;
        std::string noisyPath = rootPath + "/data/noisy/" + fname;

        SignalProcessor::Signal cleanSig, noisySig;
        try {
            cleanSig = SignalGenerator::loadSignalFromCSV(cleanPath);
            noisySig = SignalGenerator::loadSignalFromCSV(noisyPath);
        } catch (const std::exception& e) {
            std::cerr << "Пропуск " << fname << ": " << e.what() << "\n";
            continue;
        }

        std::cout << "Обработка: " << fname
                  << " (" << noisySig.size() << " отсчётов)\n";

        for (size_t ci = 0; ci < C; ++ci) {
            auto fs = configs[ci].factory();
            auto fp = configs[ci].factory();

            singleResults[ci].push_back(runSingle  (configs[ci].name, *fs, noisySig, cleanSig));
            pipeResults  [ci].push_back(runPipeline (configs[ci].name, *fp, noisySig, cleanSig));
        }
    }

    if (singleResults[0].empty()) {
        std::cerr << "Нет данных для анализа.\n";
        return 1;
    }

    // ── Сводная таблица ────────────────────────────────────────────────────
    const std::string sep(86, '-');
    std::cout << "\n" << sep << "\n";
    std::cout << std::format("{:<38} {:>8} {:>14} {:>12} {:>10}\n",
        "Конфигурация", "SNR(дБ)", "MSE", "Корреляция", "Время(мкс)");
    std::cout << sep << "\n";

    double bestSingleSNR = -1e9, bestPipeSNR = -1e9;
    std::string bestSingleLabel, bestPipeLabel;

    for (size_t ci = 0; ci < C; ++ci) {
        RunResult single = (singleResults[ci].size() > 1)
            ? average(singleResults[ci]) : singleResults[ci][0];
        RunResult pipe   = (pipeResults[ci].size() > 1)
            ? average(pipeResults[ci])   : pipeResults  [ci][0];

        // Убираем "[avg] " из label для чистоты
        single.label = configs[ci].name;
        pipe.label   = "Outlier→" + configs[ci].name;

        printRow(single);
        printRow(pipe);

        double delta = pipe.snr - single.snr;
        std::cout << std::format("  {:>38} {:>+8.2f} дБ\n", "Δ SNR (pipeline gain):", delta);
        std::cout << "\n";

        if (single.snr > bestSingleSNR) { bestSingleSNR = single.snr; bestSingleLabel = single.label; }
        if (pipe.snr   > bestPipeSNR  ) { bestPipeSNR   = pipe.snr;   bestPipeLabel   = pipe.label;   }
    }

    std::cout << sep << "\n";
    std::cout << std::format("Лучший одиночный фильтр:    {:<30} SNR={:.2f} дБ\n",
        bestSingleLabel, bestSingleSNR);
    std::cout << std::format("Лучшая двухэтапная цепочка: {:<30} SNR={:.2f} дБ\n",
        bestPipeLabel, bestPipeSNR);
    std::cout << std::format("Прирост от предфильтрации:  {:>+.2f} дБ\n",
        bestPipeSNR - bestSingleSNR);

    return 0;
}
