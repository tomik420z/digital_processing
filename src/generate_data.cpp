#include <iostream>
#include <string>

#include "signal_generator.h"

void printUsage(const char* programName) {
    std::cout << "Использование: " << programName << " [опции]\n\n";
    std::cout << "Опции:\n";
    std::cout << "  -h, --help           Показать эту справку\n";
    std::cout << "  -n, --num-signals N  Количество сигналов для генерации (по умолчанию: 10)\n";
    std::cout << "  -l, --length L       Длина каждого сигнала (по умолчанию: 1000)\n";
    std::cout << "  -s, --seed S         Начальное значение для генератора (по умолчанию: 42)\n";
    std::cout << "  -o, --output DIR     Выходная директория (по умолчанию: data)\n";
    std::cout << "\n";
    std::cout << "Пример:\n";
    std::cout << "  " << programName << " -n 50 -l 2000 -o test_data\n";
}

int main(int argc, char* argv[]) {
    // Параметры по умолчанию
    size_t numSignals = 10;
    size_t signalLength = 1000;
    unsigned int seed = 42;
    std::string outputDir = "data";

    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "-n" || arg == "--num-signals") {
            if (i + 1 < argc) {
                numSignals = std::stoul(argv[++i]);
            } else {
                std::cerr << "Ошибка: не указано количество сигналов для " << arg << std::endl;
                return 1;
            }
        }
        else if (arg == "-l" || arg == "--length") {
            if (i + 1 < argc) {
                signalLength = std::stoul(argv[++i]);
            } else {
                std::cerr << "Ошибка: не указана длина сигнала для " << arg << std::endl;
                return 1;
            }
        }
        else if (arg == "-s" || arg == "--seed") {
            if (i + 1 < argc) {
                seed = std::stoul(argv[++i]);
            } else {
                std::cerr << "Ошибка: не указано начальное значение для " << arg << std::endl;
                return 1;
            }
        }
        else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                outputDir = argv[++i];
            } else {
                std::cerr << "Ошибка: не указана выходная директория для " << arg << std::endl;
                return 1;
            }
        }
        else {
            std::cerr << "Неизвестный аргумент: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "========================================\n";
    std::cout << "   ГЕНЕРАТОР ТЕСТОВЫХ ДАННЫХ\n";
    std::cout << "========================================\n\n";

    std::cout << "Параметры генерации:\n";
    std::cout << "  Количество сигналов: " << numSignals << "\n";
    std::cout << "  Длина сигналов: " << signalLength << " отсчетов\n";
    std::cout << "  Начальное значение: " << seed << "\n";
    std::cout << "  Выходная директория: " << outputDir << "\n\n";

    try {
        // Создаем генератор сигналов
        SignalGenerator generator(seed);

        std::cout << "Генерация тестового набора данных...\n";
        auto dataset = generator.generateTestDataset(signalLength, numSignals);

        std::cout << "Сгенерировано " << dataset.size() << " пар сигналов\n";

        // Создаем директории для сохранения
        std::string cleanDir = outputDir + "/clean";
        std::string noisyDir = outputDir + "/noisy";

        std::cout << "Сохранение данных в директории:\n";
        std::cout << "  Чистые сигналы: " << cleanDir << "\n";
        std::cout << "  Зашумленные сигналы: " << noisyDir << "\n";

        // Сохраняем каждую пару сигналов
        for (size_t i = 0; i < dataset.size(); ++i) {
            std::string cleanFile = cleanDir + "/signal_" + std::to_string(i) + ".csv";
            std::string noisyFile = noisyDir + "/signal_" + std::to_string(i) + ".csv";

            SignalGenerator::saveSignalToCSV(dataset[i].first, cleanFile);
            SignalGenerator::saveSignalToCSV(dataset[i].second, noisyFile);

            if ((i + 1) % 10 == 0 || i == dataset.size() - 1) {
                std::cout << "Сохранено " << (i + 1) << "/" << dataset.size()
                          << " пар сигналов\r" << std::flush;
            }
        }

        std::cout << "\n\nГенерация тестовых данных завершена успешно!\n";

        // Дополнительная статистическая информация
        std::cout << "\nИнформация о сгенерированных данных:\n";

        // Подсчет различных типов сигналов
        std::cout << "Типы сигналов:\n";
        std::cout << "  - Прямоугольные импульсы\n";
        std::cout << "  - Треугольные импульсы\n";
        std::cout << "  - Гауссовские импульсы\n";
        std::cout << "  - Экспоненциальные импульсы\n";
        std::cout << "  - ЛЧМ импульсы\n\n";

        std::cout << "Типы помех:\n";
        std::cout << "  - Одиночные импульсы\n";
        std::cout << "  - Случайные выбросы\n";
        std::cout << "  - Пакетные помехи\n";
        std::cout << "  - Периодические импульсы\n\n";

        std::cout << "Файлы сохранены в формате CSV с колонками: Index, Value\n";
        std::cout << "Данные готовы для тестирования алгоритмов фильтрации.\n";

    } catch (const std::exception& e) {
        std::cerr << "Ошибка при генерации данных: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}