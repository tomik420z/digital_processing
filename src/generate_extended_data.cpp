/**
 * Генератор составных нелинейных сигналов длиной 4096 точек.
 *
 * Каждый сигнал — это временной ряд, где разные участки описываются
 * РАЗНЫМИ математическими функциями (сигнал меняет своё «поведение»
 * по мере течения времени). Это имитирует реальные радиолокационные
 * сигналы, которые проходят через несколько режимов за время наблюдения.
 *
 * Сигналы 0–9:
 *   0: Синус→ЛЧМ→Синус (свипирующий переход)
 *   1: Прямоугольник→Треугольник→Прямоугольник (переключение формы)
 *   2: Экспоненциальный рост + затухающая синусоида
 *   3: Несколько гауссовских импульсов с разной шириной
 *   4: Гармоническое колебание с AM-модуляцией (огибающая — синус)
 *   5: Пилообразный сигнал с нарастающей частотой
 *   6: Кусочно-линейный сигнал (трапеция + ступенька + пила)
 *   7: Синус с резким изменением фазы (прыжок π/2)
 *   8: Биение двух близких частот (beating)
 *   9: Случайный переход: SINE→SQUARE→TRIANGLE→ECHO-пульс
 *
 * Помехи: к каждому сигналу добавляется:
 *   - Гауссов белый шум (SNR ~10 дБ)
 *   - Несинхронные импульсные выбросы (плотность 2%, амплитуда 5×RMS)
 */

#include "signal_generator.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <filesystem>
#include <iomanip>

// ─────────────────────────────────────────────────────────────────────────────
// Вспомогательные функции генерации
// ─────────────────────────────────────────────────────────────────────────────

using Signal = std::vector<double>;

static constexpr size_t N = 4096;   // длина каждого сигнала
static constexpr double PI = M_PI;

/// Гауссов белый шум с заданной дисперсией
static Signal whiteNoise(double variance, std::mt19937& rng)
{
    Signal n(N, 0.0);
    std::normal_distribution<double> dist(0.0, std::sqrt(variance));
    for (auto& v : n) v = dist(rng);
    return n;
}

/// RMS сигнала
static double rms(const Signal& s)
{
    double sum = 0.0;
    for (double v : s) sum += v * v;
    return std::sqrt(sum / static_cast<double>(s.size()));
}

/// Добавить несинхронные импульсные выбросы
static void addImpulses(Signal& s, double density, double ampScale, std::mt19937& rng)
{
    const double sigRms = rms(s);
    std::uniform_real_distribution<double> ud(0.0, 1.0);
    std::normal_distribution<double>       nd(0.0, 1.0);
    for (size_t i = 0; i < N; ++i) {
        if (ud(rng) < density) {
            // Случайный знак и случайная амплитуда в диапазоне [1, 2]×ampScale×RMS
            double amp = (1.0 + ud(rng)) * ampScale * sigRms;
            s[i] += (nd(rng) > 0 ? 1.0 : -1.0) * amp;
        }
    }
}

/// Сохранить сигнал в CSV
static void saveCSV(const Signal& s, const std::string& path)
{
    // Создаём директорию если не существует
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path());

    std::ofstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    f << "Index,Value\n";
    for (size_t i = 0; i < s.size(); ++i)
        f << i << "," << s[i] << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// 10 составных нелинейных сигналов
// ─────────────────────────────────────────────────────────────────────────────

/// 0: Синус → ЛЧМ → Синус обратно
/// Участок 1 [0, N/3):     A·sin(2π·f0·n)
/// Участок 2 [N/3, 2N/3):  A·sin(2π·(f0 + (f1-f0)·t/T)·n)  — линейный свип
/// Участок 3 [2N/3, N):    A·sin(2π·f1·n)
static Signal signal0()
{
    Signal s(N);
    const double f0 = 0.02, f1 = 0.10, A = 1.0;
    const double seg = N / 3.0;
    for (size_t n = 0; n < N; ++n) {
        double t = static_cast<double>(n);
        double val;
        if (n < static_cast<size_t>(seg)) {
            val = A * std::sin(2*PI*f0*t);
        } else if (n < static_cast<size_t>(2*seg)) {
            double tt = t - seg;
            double ft = f0 + (f1-f0) * tt / seg;
            val = A * std::sin(2*PI*ft*t);
        } else {
            val = A * std::sin(2*PI*f1*t);
        }
        s[n] = val;
    }
    return s;
}

/// 1: Прямоугольник → Треугольник → Прямоугольник
/// Участок 1: меандр с частотой f=0.03
/// Участок 2: треугольный сигнал f=0.03
/// Участок 3: меандр с вдвое большей частотой f=0.06
static Signal signal1()
{
    Signal s(N);
    const double f1 = 0.03, f2 = 0.06;
    const size_t seg = N / 3;
    for (size_t n = 0; n < N; ++n) {
        double t = static_cast<double>(n);
        if (n < seg) {
            // Меандр
            s[n] = (std::fmod(t * f1, 1.0) < 0.5) ? 1.0 : -1.0;
        } else if (n < 2*seg) {
            // Треугольник
            double phase = std::fmod(t * f1, 1.0);
            s[n] = (phase < 0.5) ? (4*phase - 1.0) : (3.0 - 4*phase);
        } else {
            // Меандр быстрый
            s[n] = (std::fmod(t * f2, 1.0) < 0.5) ? 1.0 : -1.0;
        }
    }
    return s;
}

/// 2: Экспоненциальный рост + затухающая синусоида
/// [0, N/2): A·(1 - exp(-t/τ))·sin(2π·f·t)  — нарастающая
/// [N/2, N): A·exp(-(t-N/2)/τ)·sin(2π·f·t)  — затухающая
static Signal signal2()
{
    Signal s(N);
    const double f = 0.04, A = 1.0, tau = N / 6.0;
    const size_t half = N / 2;
    for (size_t n = 0; n < N; ++n) {
        double t = static_cast<double>(n);
        double env;
        if (n < half) {
            env = 1.0 - std::exp(-t / tau);
        } else {
            double tt = t - static_cast<double>(half);
            env = std::exp(-tt / tau);
        }
        s[n] = A * env * std::sin(2*PI*f*t);
    }
    return s;
}

/// 3: Серия гауссовских импульсов с разными параметрами
/// 8 импульсов в позициях N/9, 2N/9, ... 8N/9 с разными σ и амплитудами
static Signal signal3()
{
    Signal s(N, 0.0);
    const int numPulses = 8;
    const double positions[] = {0.1, 0.2, 0.3, 0.42, 0.55, 0.65, 0.78, 0.9};
    const double widths[]    = {30,  50,  20,  80,   35,   60,   25,   45};
    const double amps[]      = {1.0, 0.7, 1.3, 0.5,  1.1,  0.8,  1.5,  0.6};
    for (size_t n = 0; n < N; ++n) {
        double t = static_cast<double>(n) / N;
        for (int p = 0; p < numPulses; ++p) {
            double dt = (t - positions[p]) * N;
            s[n] += amps[p] * std::exp(-dt*dt / (2*widths[p]*widths[p]));
        }
    }
    return s;
}

/// 4: Синус с AM-модуляцией (огибающая — медленный синус)
/// s[n] = (1 + m·sin(2π·fm·n)) · sin(2π·fc·n)
/// fc = 0.1, fm = 0.005, m = 0.8
static Signal signal4()
{
    Signal s(N);
    const double fc = 0.10, fm = 0.005, m = 0.8, A = 1.0;
    for (size_t n = 0; n < N; ++n) {
        double t = static_cast<double>(n);
        double env = 1.0 + m * std::sin(2*PI*fm*t);
        s[n] = A * env * std::sin(2*PI*fc*t);
    }
    return s;
}

/// 5: Пилообразный с нарастающей частотой (нелинейный chirp вручную)
/// Частота линейно возрастает от f0 до f1 через весь сигнал.
/// Аналог: s[n] = sawtooth(2π·(f0 + (f1-f0)·n/(2N))·n)
static Signal signal5()
{
    Signal s(N);
    const double f0 = 0.01, f1 = 0.12;
    for (size_t n = 0; n < N; ++n) {
        double t = static_cast<double>(n);
        double ft = f0 + (f1-f0) * t / static_cast<double>(N);
        // Нормированная фаза
        double phase = std::fmod(ft * t, 1.0);
        s[n] = 2.0 * phase - 1.0;  // пила в диапазоне [-1, 1]
    }
    return s;
}

/// 6: Кусочно-линейный: трапеция + ступенька + пила
/// [0, N/4):   трапеция (нарастание, плато, спад)
/// [N/4, N/2): ступенчатая функция (3 уровня)
/// [N/2, 3N/4): пилообразный с f=0.04
/// [3N/4, N):  нулевой фон + один прямоугольный импульс
static Signal signal6()
{
    Signal s(N, 0.0);
    const size_t Q = N / 4;
    for (size_t n = 0; n < Q; ++n) {
        // Трапеция
        const double edge = Q / 4.0;
        if (n < static_cast<size_t>(edge))
            s[n] = static_cast<double>(n) / edge;
        else if (n < static_cast<size_t>(3*edge))
            s[n] = 1.0;
        else
            s[n] = 1.0 - (static_cast<double>(n) - 3*edge) / edge;
    }
    for (size_t n = Q; n < 2*Q; ++n) {
        // Ступеньки
        double pos = static_cast<double>(n - Q) / Q;
        if (pos < 0.33) s[n] = -0.5;
        else if (pos < 0.66) s[n] = 0.5;
        else s[n] = 1.0;
    }
    for (size_t n = 2*Q; n < 3*Q; ++n) {
        // Пила
        double phase = std::fmod(0.04 * static_cast<double>(n), 1.0);
        s[n] = 2.0*phase - 1.0;
    }
    for (size_t n = 3*Q; n < N; ++n) {
        // Прямоугольный импульс в середине четвертого сегмента
        const size_t mid = 3*Q + Q/2;
        const size_t hw  = Q/8;
        s[n] = (n >= mid - hw && n < mid + hw) ? 1.0 : 0.0;
    }
    return s;
}

/// 7: Синус с резким прыжком фазы на π/2 в двух точках
/// [0, N/3):    A·sin(2π·f·n)
/// [N/3, 2N/3): A·sin(2π·f·n + π/2) = A·cos(2π·f·n)
/// [2N/3, N):   A·sin(2π·f·n + π)   = -A·sin(2π·f·n)
static Signal signal7()
{
    Signal s(N);
    const double f = 0.05, A = 1.0;
    const size_t seg = N / 3;
    for (size_t n = 0; n < N; ++n) {
        double t = static_cast<double>(n);
        double phase;
        if (n < seg)      phase = 0.0;
        else if (n < 2*seg) phase = PI/2;
        else              phase = PI;
        s[n] = A * std::sin(2*PI*f*t + phase);
    }
    return s;
}

/// 8: Биение двух близких частот (beating)
/// s[n] = A·sin(2π·f1·n) + A·sin(2π·f2·n)
/// Огибающая имеет период T = 1/(f2-f1)
static Signal signal8()
{
    Signal s(N);
    const double f1 = 0.08, f2 = 0.085, A = 0.5;
    for (size_t n = 0; n < N; ++n) {
        double t = static_cast<double>(n);
        s[n] = A * std::sin(2*PI*f1*t) + A * std::sin(2*PI*f2*t);
    }
    return s;
}

/// 9: Случайный переход четырёх типов: SINE → SQUARE → TRIANGLE → GAUSSIAN PULSE
/// [0, N/4):    A·sin(2π·f·n)
/// [N/4, N/2):  меандр f=0.04
/// [N/2, 3N/4): треугольный f=0.04
/// [3N/4, N):   одиночный широкий гауссовский импульс
static Signal signal9()
{
    Signal s(N, 0.0);
    const size_t Q = N / 4;
    const double f = 0.04, A = 1.0;
    for (size_t n = 0; n < Q; ++n) {
        s[n] = A * std::sin(2*PI*f*static_cast<double>(n));
    }
    for (size_t n = Q; n < 2*Q; ++n) {
        s[n] = (std::fmod(f*static_cast<double>(n), 1.0) < 0.5) ? A : -A;
    }
    for (size_t n = 2*Q; n < 3*Q; ++n) {
        double phase = std::fmod(f*static_cast<double>(n), 1.0);
        s[n] = A * ((phase < 0.5) ? (4*phase - 1.0) : (3.0 - 4*phase));
    }
    // Гауссовский пульс — центр в конце отрезка
    const double center = 3.5 * Q;
    const double sigma  = Q / 4.0;
    for (size_t n = 3*Q; n < N; ++n) {
        double dt = static_cast<double>(n) - center;
        s[n] = A * std::exp(-dt*dt / (2*sigma*sigma));
    }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    std::string outDir = "data/extended";
    unsigned int seed  = 123;
    double noiseSNR_dB = 10.0;   // SNR гауссова шума, дБ
    double impulseRate = 0.02;   // плотность импульсных выбросов (2%)
    double impulseAmp  = 5.0;    // амплитуда выброса в единицах RMS

    // Простой парсер аргументов
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-o" || a == "--output") && i+1 < argc)
            outDir = argv[++i];
        else if ((a == "-s" || a == "--seed") && i+1 < argc)
            seed = static_cast<unsigned>(std::stoul(argv[++i]));
        else if ((a == "--snr") && i+1 < argc)
            noiseSNR_dB = std::stod(argv[++i]);
        else if ((a == "--impulse-rate") && i+1 < argc)
            impulseRate = std::stod(argv[++i]);
        else if ((a == "--impulse-amp") && i+1 < argc)
            impulseAmp = std::stod(argv[++i]);
    }

    std::cout << "================================================\n";
    std::cout << "  ГЕНЕРАТОР СОСТАВНЫХ НЕЛИНЕЙНЫХ СИГНАЛОВ\n";
    std::cout << "================================================\n\n";
    std::cout << "Длина сигналов:  " << N << " точек\n";
    std::cout << "SNR гауссов шум: " << noiseSNR_dB << " дБ\n";
    std::cout << "Плотность помех: " << impulseRate*100 << " %\n";
    std::cout << "Амплитуда помех: " << impulseAmp << " × RMS\n";
    std::cout << "Выходная папка:  " << outDir << "\n\n";

    std::mt19937 rng(seed);

    // Описания сигналов
    const std::vector<std::string> descriptions = {
        "0: Sine → LFM chirp → Sine",
        "1: Square → Triangle → Fast Square",
        "2: Exponential envelope × Sine (rise+decay)",
        "3: Multiple Gaussian pulses (different widths)",
        "4: AM-modulated Sine (carrier 0.1, mod 0.005)",
        "5: Sawtooth with increasing frequency (chirp-like)",
        "6: Trapezoid + Steps + Sawtooth + Rectangular pulse",
        "7: Sine with sudden phase jumps (0 → π/2 → π)",
        "8: Two-frequency beating (f1=0.08, f2=0.085)",
        "9: Multi-type: Sine → Square → Triangle → Gaussian pulse"
    };

    // Функции генерации чистых сигналов
    using GenFn = Signal(*)();
    const std::vector<GenFn> generators = {
        signal0, signal1, signal2, signal3, signal4,
        signal5, signal6, signal7, signal8, signal9
    };

    const std::string cleanDir = outDir + "/clean";
    const std::string noisyDir = outDir + "/noisy";

    for (size_t i = 0; i < generators.size(); ++i) {
        std::cout << "Генерация сигнала " << i << ": " << descriptions[i] << "\n";

        // Чистый сигнал
        Signal clean = generators[i]();

        // Вычисляем мощность чистого сигнала для расчёта шума
        const double sigPow = [&]{
            double s = 0.0;
            for (double v : clean) s += v*v;
            return s / static_cast<double>(N);
        }();

        // Мощность шума из SNR
        const double snrLinear = std::pow(10.0, noiseSNR_dB / 10.0);
        const double noisePow  = sigPow / snrLinear;

        // Копируем чистый → зашумлённый
        Signal noisy = clean;

        // 1. Гауссов белый шум
        Signal noise = whiteNoise(noisePow, rng);
        for (size_t n = 0; n < N; ++n)
            noisy[n] += noise[n];

        // 2. Несинхронные импульсные выбросы
        addImpulses(noisy, impulseRate, impulseAmp, rng);

        // Сохраняем
        const std::string cleanFile = cleanDir + "/signal_" + std::to_string(i) + ".csv";
        const std::string noisyFile = noisyDir + "/signal_" + std::to_string(i) + ".csv";

        saveCSV(clean, cleanFile);
        saveCSV(noisy, noisyFile);

        // Вычисляем фактический SNR
        double noise2 = 0.0;
        for (size_t n = 0; n < N; ++n) {
            double d = noisy[n] - clean[n];
            noise2 += d*d;
        }
        const double actualSNR = 10.0 * std::log10(
            sigPow / (noise2 / static_cast<double>(N)));

        std::cout << "  clean: " << cleanFile
                  << "  noisy: " << noisyFile
                  << "  фактический SNR: " << std::fixed
                  << std::setprecision(1) << actualSNR << " дБ\n";
    }

    std::cout << "\nГотово! Сгенерировано " << generators.size()
              << " пар сигналов по " << N << " точек.\n";
    std::cout << "Используйте:\n";
    std::cout << "  ./signal_filter_gui -f spectral -i " << noisyDir
              << "/signal_0.csv -c " << cleanDir << "/signal_0.csv\n";
    std::cout << "  ./signal_filter_gui -f auto -i " << noisyDir
              << "/signal_0.csv -c " << cleanDir << "/signal_0.csv\n";

    return 0;
}
