/**
 * Генератор специализированных сигналов для тестирования фильтра Винера.
 *
 * Ключевое предположение фильтра Винера:
 *   — истинный сигнал медленно меняется (низкочастотный, гладкий)
 *   — помехи высокочастотные или импульсные (отличаются от сигнала по спектру)
 *
 * Генерируется 8 пар (clean / noisy):
 *   0: Медленный синус + гауссов белый шум (SNR 10 дБ)
 *   1: Плавная экспоненциальная кривая + гауссов белый шум
 *   2: Полиномиальный тренд (парабола) + гауссов белый шум
 *   3: Сумма трёх медленных синусов + гауссов белый шум
 *   4: Медленный синус + импульсные выбросы (плотность 2%)
 *   5: Затухающая синусоида + импульсные выбросы
 *   6: Ступенчатая функция (плавные переходы) + смешанный шум
 *   7: Медленный линейный тренд + гауссов шум + редкие импульсы
 */

#include "signal_generator.h"
#include <iostream>
#include <filesystem>
#include <string>

static void printUsage(const char* prog) {
    std::cout << "Использование: " << prog << " [опции]\n\n"
              << "Опции:\n"
              << "  -h, --help              Показать эту справку\n"
              << "  -l, --length N          Длина каждого сигнала (по умолчанию: 1024)\n"
              << "  -s, --seed S            Seed генератора случайных чисел (по умолчанию: 7)\n"
              << "  --snr DB                SNR гауссова шума в дБ (по умолчанию: 10.0)\n"
              << "  --impulse-rate R        Плотность импульсных выбросов 0..1 (по умолчанию: 0.02)\n"
              << "  --impulse-amp A         Амплитуда выбросов в единицах RMS (по умолчанию: 5.0)\n"
              << "  -o, --output DIR        Выходная директория (по умолчанию: data/wiener)\n"
              << "\nОписание сигналов:\n"
              << "  0: Медленный синус (f=0.005) + гауссов шум\n"
              << "  1: Экспоненциальная кривая + гауссов шум\n"
              << "  2: Парабола (полиномиальный тренд) + гауссов шум\n"
              << "  3: Три медленных синуса + гауссов шум\n"
              << "  4: Медленный синус + импульсные выбросы\n"
              << "  5: Затухающая синусоида + импульсные выбросы\n"
              << "  6: Ступенчатая функция (сглаженная) + смешанный шум\n"
              << "  7: Линейный тренд + гауссов шум + редкие импульсы\n";
}

int main(int argc, char* argv[]) {
    size_t      signalLength    = 1024;
    unsigned int seed           = 7;
    double      gaussianSNR_dB  = 10.0;
    double      impulseDensity  = 0.02;
    double      impulseAmplitude = 5.0;
    std::string outputDir       = "data/wiener";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if ((a == "-l" || a == "--length") && i + 1 < argc) {
            signalLength = std::stoul(argv[++i]);
        } else if ((a == "-s" || a == "--seed") && i + 1 < argc) {
            seed = static_cast<unsigned int>(std::stoul(argv[++i]));
        } else if (a == "--snr" && i + 1 < argc) {
            gaussianSNR_dB = std::stod(argv[++i]);
        } else if (a == "--impulse-rate" && i + 1 < argc) {
            impulseDensity = std::stod(argv[++i]);
        } else if (a == "--impulse-amp" && i + 1 < argc) {
            impulseAmplitude = std::stod(argv[++i]);
        } else if ((a == "-o" || a == "--output") && i + 1 < argc) {
            outputDir = argv[++i];
        } else {
            std::cerr << "Неизвестный аргумент: " << a << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "============================================\n"
              << "  ГЕНЕРАТОР ДАННЫХ ДЛЯ ФИЛЬТРА ВИНЕРА\n"
              << "============================================\n\n"
              << "Параметры:\n"
              << "  Длина сигналов:       " << signalLength  << " отсчётов\n"
              << "  Seed:                 " << seed          << "\n"
              << "  SNR гауссов шум:      " << gaussianSNR_dB << " дБ\n"
              << "  Плотность выбросов:   " << impulseDensity * 100.0 << " %\n"
              << "  Амплитуда выбросов:   " << impulseAmplitude << " × RMS\n"
              << "  Выходная директория:  " << outputDir     << "\n\n";

    const std::vector<std::string> descriptions = {
        "0: Медленный синус (f=0.005) + гауссов белый шум",
        "1: Экспоненциальная кривая + гауссов белый шум",
        "2: Парабола (полиномиальный тренд) + гауссов белый шум",
        "3: Три медленных синуса + гауссов белый шум",
        "4: Медленный синус + импульсные выбросы (плотность 2%)",
        "5: Затухающая синусоида + импульсные выбросы",
        "6: Ступенчатая функция (сглаженная) + смешанный шум",
        "7: Линейный тренд + гауссов шум + редкие импульсы"
    };

    try {
        SignalGenerator gen(seed);
        auto dataset = gen.generateWienerTestDataset(
            signalLength, gaussianSNR_dB, impulseDensity, impulseAmplitude);

        const std::string cleanDir = outputDir + "/clean";
        const std::string noisyDir = outputDir + "/noisy";

        std::filesystem::create_directories(cleanDir);
        std::filesystem::create_directories(noisyDir);

        std::cout << "Сохранение:\n";
        for (size_t i = 0; i < dataset.size(); ++i) {
            std::string cleanFile = cleanDir + "/signal_" + std::to_string(i) + ".csv";
            std::string noisyFile = noisyDir + "/signal_" + std::to_string(i) + ".csv";

            SignalGenerator::saveSignalToCSV(dataset[i].first,  cleanFile);
            SignalGenerator::saveSignalToCSV(dataset[i].second, noisyFile);

            std::cout << "  [" << i << "] " << descriptions[i] << "\n"
                      << "       clean -> " << cleanFile << "\n"
                      << "       noisy -> " << noisyFile << "\n";
        }

        std::cout << "\nГотово! Сгенерировано " << dataset.size()
                  << " пар сигналов по " << signalLength << " отсчётов.\n\n"
                  << "Использование с фильтром Винера:\n"
                  << "  ./echo_filter_test -f wiener -i " << noisyDir
                  << "/signal_0.csv -c " << cleanDir << "/signal_0.csv\n"
                  << "  ./signal_filter_gui -f wiener -i " << noisyDir
                  << "/signal_0.csv -c " << cleanDir << "/signal_0.csv\n";

    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
