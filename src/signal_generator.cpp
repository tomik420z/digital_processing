#include "signal_generator.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

SignalGenerator::SignalGenerator(unsigned int seed) : rng_(seed) {
}

SignalProcessor::Signal SignalGenerator::generateBasicSignal(SignalType type,
                                                             size_t length,
                                                             double amplitude,
                                                             double frequency,
                                                             double phase,
                                                             double dutyCycle) const {
    switch (type) {
        case SignalType::SINE:
            return generateSineSignal(length, amplitude, frequency, phase);
        case SignalType::SQUARE:
            return generateSquareSignal(length, amplitude, frequency, phase, dutyCycle);
        case SignalType::TRIANGLE:
            return generateTriangleSignal(length, amplitude, frequency, phase);
        case SignalType::SAWTOOTH:
            return generateSawtoothSignal(length, amplitude, frequency, phase);
        default:
            return generateSineSignal(length, amplitude, frequency, phase);
    }
}

SignalProcessor::Signal SignalGenerator::generateEchoSignal(EchoType type,
                                                            size_t length,
                                                            double amplitude,
                                                            size_t echoDelay,
                                                            double echoAttenuation,
                                                            double noiseLevel) const {
    Signal signal(length, 0.0);

    // Определяем длину основного импульса (примерно 10% от общей длины)
    size_t pulseLength = std::max(size_t(1), length / 10);

    // Генерируем основной импульс
    Signal mainPulse = generatePulse(type, pulseLength, amplitude);

    // Размещаем основной импульс в начале сигнала
    size_t mainStart = length / 20; // Начинаем с небольшой задержкой
    for (size_t i = 0; i < mainPulse.size() && (mainStart + i) < length; ++i) {
        signal[mainStart + i] = mainPulse[i];
    }

    // Добавляем эхо
    if (echoDelay < length && echoAttenuation > 0.0) {
        size_t echoStart = mainStart + echoDelay;
        for (size_t i = 0; i < mainPulse.size() && (echoStart + i) < length; ++i) {
            signal[echoStart + i] += mainPulse[i] * echoAttenuation;
        }
    }

    // Добавляем фоновый шум
    if (noiseLevel > 0.0) {
        Signal noise = generateWhiteNoise(length, noiseLevel * noiseLevel);
        for (size_t i = 0; i < length; ++i) {
            signal[i] += noise[i];
        }
    }

    return signal;
}

SignalProcessor::Signal SignalGenerator::generateImpulseNoise(size_t length,
                                                              NoiseType type,
                                                              double density,
                                                              double amplitude,
                                                              size_t burstLength) const {
    Signal noise(length, 0.0);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    std::normal_distribution<double> normal(0.0, 1.0);

    switch (type) {
        case NoiseType::IMPULSE: {
            // Одиночные импульсы
            for (size_t i = 0; i < length; ++i) {
                if (uniform(rng_) < density) {
                    noise[i] = amplitude * (uniform(rng_) > 0.5 ? 1.0 : -1.0);
                }
            }
            break;
        }

        case NoiseType::RANDOM_SPIKES: {
            // Случайные выбросы с различной амплитудой
            for (size_t i = 0; i < length; ++i) {
                if (uniform(rng_) < density) {
                    double randomAmplitude = amplitude * (0.5 + 0.5 * uniform(rng_));
                    noise[i] = randomAmplitude * (uniform(rng_) > 0.5 ? 1.0 : -1.0);
                }
            }
            break;
        }

        case NoiseType::BURST: {
            // Пакетные помехи
            size_t i = 0;
            while (i < length) {
                if (uniform(rng_) < density) {
                    // Начинаем пакет помех
                    for (size_t j = 0; j < burstLength && (i + j) < length; ++j) {
                        noise[i + j] = amplitude * normal(rng_);
                    }
                    i += burstLength;
                } else {
                    ++i;
                }
            }
            break;
        }

        case NoiseType::PERIODIC: {
            // Периодические импульсы
            size_t period = static_cast<size_t>(1.0 / density);
            if (period > 0) {
                for (size_t i = 0; i < length; i += period) {
                    if (i < length) {
                        noise[i] = amplitude * (uniform(rng_) > 0.5 ? 1.0 : -1.0);
                    }
                }
            }
            break;
        }
    }

    return noise;
}

SignalProcessor::Signal SignalGenerator::addImpulseNoise(const Signal& signal,
                                                         NoiseType noiseType,
                                                         double density,
                                                         double amplitude) const {
    Signal noise = generateImpulseNoise(signal.size(), noiseType, density, amplitude);
    Signal noisySignal = signal;

    for (size_t i = 0; i < signal.size(); ++i) {
        noisySignal[i] += noise[i];
    }

    return noisySignal;
}

SignalProcessor::Signal SignalGenerator::generateWhiteNoise(size_t length, double variance) const {
    Signal noise;
    noise.reserve(length);

    std::normal_distribution<double> normal(0.0, std::sqrt(variance));

    for (size_t i = 0; i < length; ++i) {
        noise.push_back(normal(rng_));
    }

    return noise;
}

std::vector<std::pair<SignalProcessor::Signal, SignalProcessor::Signal>>
SignalGenerator::generateTestDataset(size_t signalLength, size_t numSignals) const {

    std::vector<std::pair<Signal, Signal>> dataset;
    dataset.reserve(numSignals);

    // Различные типы основных сигналов
    std::vector<SignalType> signalTypes;
    signalTypes.push_back(SignalType::SINE);
    signalTypes.push_back(SignalType::SQUARE);
    signalTypes.push_back(SignalType::TRIANGLE);
    signalTypes.push_back(SignalType::SAWTOOTH);

    // Различные типы эхо сигналов (для разнообразия)
    std::vector<EchoType> echoTypes;
    echoTypes.push_back(EchoType::RECTANGULAR);
    echoTypes.push_back(EchoType::TRIANGULAR);
    echoTypes.push_back(EchoType::GAUSSIAN);
    echoTypes.push_back(EchoType::EXPONENTIAL);
    echoTypes.push_back(EchoType::CHIRP);

    // Различные типы помех
    std::vector<NoiseType> noiseTypes;
    noiseTypes.push_back(NoiseType::IMPULSE);
    noiseTypes.push_back(NoiseType::RANDOM_SPIKES);
    noiseTypes.push_back(NoiseType::BURST);
    noiseTypes.push_back(NoiseType::PERIODIC);

    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    for (size_t i = 0; i < numSignals; ++i) {
        Signal cleanSignal;

        // Генерируем половину сигналов как основные типы, половину как эхо-сигналы
        if (i % 2 == 0 && i / 2 < signalTypes.size()) {
            // Основные сигналы
            SignalType signalType = signalTypes[(i / 2) % signalTypes.size()];
            double amplitude = 0.5 + 0.5 * uniform(rng_);
            double frequency = 0.05 + 0.15 * uniform(rng_); // Частота от 0.05 до 0.2
            double phase = 2.0 * M_PI * uniform(rng_);
            double dutyCycle = 0.3 + 0.4 * uniform(rng_); // Для квадратного сигнала

            cleanSignal = generateBasicSignal(signalType, signalLength, amplitude,
                                            frequency, phase, dutyCycle);
        } else {
            // Эхо сигналы (как было раньше)
            EchoType echoType = echoTypes[i % echoTypes.size()];
            double amplitude = 0.5 + 0.5 * uniform(rng_);
            size_t echoDelay = 50 + static_cast<size_t>(100 * uniform(rng_));
            double echoAttenuation = 0.3 + 0.4 * uniform(rng_);
            double noiseLevel = 0.01 + 0.04 * uniform(rng_);

            cleanSignal = generateEchoSignal(echoType, signalLength, amplitude,
                                           echoDelay, echoAttenuation, noiseLevel);
        }

        // Добавляем импульсные помехи
        NoiseType noiseType = noiseTypes[i % noiseTypes.size()];
        double noiseDensity = 0.005 + 0.02 * uniform(rng_);
        double noiseAmplitude = 1.0 + 2.0 * uniform(rng_);

        Signal noisySignal = addImpulseNoise(cleanSignal, noiseType, noiseDensity, noiseAmplitude);

        dataset.emplace_back(cleanSignal, noisySignal);
    }

    return dataset;
}

SignalProcessor::Signal SignalGenerator::generatePulse(EchoType type, size_t length, double amplitude) const {
    switch (type) {
        case EchoType::RECTANGULAR:
            return generateRectangularPulse(length, amplitude);
        case EchoType::TRIANGULAR:
            return generateTriangularPulse(length, amplitude);
        case EchoType::GAUSSIAN:
            return generateGaussianPulse(length, amplitude);
        case EchoType::EXPONENTIAL:
            return generateExponentialPulse(length, amplitude);
        case EchoType::CHIRP:
            return generateChirpPulse(length, amplitude);
        default:
            return generateRectangularPulse(length, amplitude);
    }
}

SignalProcessor::Signal SignalGenerator::generateRectangularPulse(size_t length, double amplitude) const {
    return Signal(length, amplitude);
}

SignalProcessor::Signal SignalGenerator::generateTriangularPulse(size_t length, double amplitude) const {
    Signal pulse;
    pulse.reserve(length);

    size_t halfLength = length / 2;

    // Восходящая часть
    for (size_t i = 0; i < halfLength; ++i) {
        pulse.push_back(amplitude * static_cast<double>(i) / halfLength);
    }

    // Нисходящая часть
    for (size_t i = halfLength; i < length; ++i) {
        pulse.push_back(amplitude * static_cast<double>(length - 1 - i) / (length - halfLength));
    }

    return pulse;
}

SignalProcessor::Signal SignalGenerator::generateGaussianPulse(size_t length, double amplitude) const {
    Signal pulse;
    pulse.reserve(length);

    double sigma = static_cast<double>(length) / 6.0; // 3-sigma правило
    double center = static_cast<double>(length - 1) / 2.0;

    for (size_t i = 0; i < length; ++i) {
        double x = static_cast<double>(i) - center;
        double value = amplitude * std::exp(-0.5 * (x * x) / (sigma * sigma));
        pulse.push_back(value);
    }

    return pulse;
}

SignalProcessor::Signal SignalGenerator::generateExponentialPulse(size_t length, double amplitude) const {
    Signal pulse;
    pulse.reserve(length);

    double tau = static_cast<double>(length) / 3.0; // Постоянная времени

    for (size_t i = 0; i < length; ++i) {
        double x = static_cast<double>(i);
        double value = amplitude * std::exp(-x / tau);
        pulse.push_back(value);
    }

    return pulse;
}

SignalProcessor::Signal SignalGenerator::generateChirpPulse(size_t length, double amplitude) const {
    Signal pulse;
    pulse.reserve(length);

    // ЛЧМ импульс с линейно изменяющейся частотой
    double f0 = 0.1;  // Начальная частота (нормированная)
    double f1 = 0.5;  // Конечная частота
    double beta = (f1 - f0) / static_cast<double>(length);

    for (size_t i = 0; i < length; ++i) {
        double t = static_cast<double>(i);
        double instantFreq = f0 + beta * t;
        double phase = 2.0 * M_PI * (f0 * t + 0.5 * beta * t * t);

        // Применяем оконную функцию Ханна для сглаживания краев
        double window = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (length - 1)));
        double value = amplitude * window * std::sin(phase);

        pulse.push_back(value);
    }

    return pulse;
}

SignalProcessor::Signal SignalGenerator::generateSineSignal(size_t length, double amplitude,
                                                            double frequency, double phase) const {
    Signal signal;
    signal.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        double t = static_cast<double>(i);
        double value = amplitude * std::sin(2.0 * M_PI * frequency * t + phase);
        signal.push_back(value);
    }

    return signal;
}

SignalProcessor::Signal SignalGenerator::generateSquareSignal(size_t length, double amplitude,
                                                              double frequency, double phase,
                                                              double dutyCycle) const {
    Signal signal;
    signal.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        double t = static_cast<double>(i);
        double phase_t = std::fmod(2.0 * M_PI * frequency * t + phase, 2.0 * M_PI);
        if (phase_t < 0) phase_t += 2.0 * M_PI;

        double value = (phase_t < 2.0 * M_PI * dutyCycle) ? amplitude : -amplitude;
        signal.push_back(value);
    }

    return signal;
}

SignalProcessor::Signal SignalGenerator::generateTriangleSignal(size_t length, double amplitude,
                                                                double frequency, double phase) const {
    Signal signal;
    signal.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        double t = static_cast<double>(i);
        double phase_t = std::fmod(2.0 * M_PI * frequency * t + phase, 2.0 * M_PI);
        if (phase_t < 0) phase_t += 2.0 * M_PI;

        double value;
        if (phase_t < M_PI) {
            // Восходящая часть: от -amplitude до +amplitude
            value = amplitude * (2.0 * phase_t / M_PI - 1.0);
        } else {
            // Нисходящая часть: от +amplitude до -amplitude
            value = amplitude * (3.0 - 2.0 * phase_t / M_PI);
        }
        signal.push_back(value);
    }

    return signal;
}

SignalProcessor::Signal SignalGenerator::generateSawtoothSignal(size_t length, double amplitude,
                                                                double frequency, double phase) const {
    Signal signal;
    signal.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        double t = static_cast<double>(i);
        double phase_t = std::fmod(2.0 * M_PI * frequency * t + phase, 2.0 * M_PI);
        if (phase_t < 0) phase_t += 2.0 * M_PI;

        // Линейно возрастающий сигнал от -amplitude до +amplitude
        double value = amplitude * (2.0 * phase_t / (2.0 * M_PI) - 1.0);
        signal.push_back(value);
    }

    return signal;
}

void SignalGenerator::saveSignalToCSV(const Signal& signal, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    file << "Index,Value\n";
    for (size_t i = 0; i < signal.size(); ++i) {
        file << i << "," << signal[i] << "\n";
    }

    file.close();
}

SignalProcessor::Signal SignalGenerator::loadSignalFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for reading: " + filename);
    }

    Signal signal;
    std::string line;

    // Пропускаем заголовок
    if (std::getline(file, line)) {
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string indexStr, valueStr;

            if (std::getline(ss, indexStr, ',') && std::getline(ss, valueStr)) {
                try {
                    double value = std::stod(valueStr);
                    signal.push_back(value);
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing line: " << line << std::endl;
                }
            }
        }
    }

    file.close();
    return signal;
}

std::string SignalGenerator::signalTypeToString(SignalType type) {
    switch (type) {
        case SignalType::SINE: return "Sine";
        case SignalType::SQUARE: return "Square";
        case SignalType::TRIANGLE: return "Triangle";
        case SignalType::SAWTOOTH: return "Sawtooth";
        default: return "Unknown";
    }
}

std::string SignalGenerator::echoTypeToString(EchoType type) {
    switch (type) {
        case EchoType::RECTANGULAR: return "Rectangular";
        case EchoType::TRIANGULAR: return "Triangular";
        case EchoType::GAUSSIAN: return "Gaussian";
        case EchoType::EXPONENTIAL: return "Exponential";
        case EchoType::CHIRP: return "Chirp";
        default: return "Unknown";
    }
}

std::string SignalGenerator::noiseTypeToString(NoiseType type) {
    switch (type) {
        case NoiseType::IMPULSE: return "Impulse";
        case NoiseType::BURST: return "Burst";
        case NoiseType::RANDOM_SPIKES: return "RandomSpikes";
        case NoiseType::PERIODIC: return "Periodic";
        default: return "Unknown";
    }
}