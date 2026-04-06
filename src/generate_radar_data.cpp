#include "doppler_nip_filter.h"
#include "signal_processor.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <complex>
#include <cmath>
#include <random>
#include <iomanip>
#include <sys/stat.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Описание одной цели в пачке ─────────────────────────────────────────────
struct Target {
    double amplitude;   ///< Амплитуда эхо-сигнала
    double dopplerFreq; ///< Нормированная доплеровская частота (доля F_PRF)
    double initPhase;   ///< Начальная фаза (рад)
};

/**
 * Генерирует пачку из N комплексных отсчётов I+jQ для одного дискрета дальности.
 *
 * Модель:  x[n] = Σ_i A_i · exp(j·(2π·f_d_i·n + φ_i)) + η[n]
 *
 * @param N         Число импульсов в пачке
 * @param targets   Список целей
 * @param noiseStd  СКО белого шума приёмника (I и Q независимо)
 * @param rng       ГПСЧ
 */
CVector generateBurst(int N,
                      const std::vector<Target>& targets,
                      double noiseStd,
                      std::mt19937& rng)
{
    std::normal_distribution<double> noiseDist(0.0, noiseStd);
    CVector burst(static_cast<size_t>(N));

    for (int n = 0; n < N; ++n) {
        Complex sample(noiseDist(rng), noiseDist(rng));

        for (size_t t = 0; t < targets.size(); ++t) {
            const double amp  = targets[t].amplitude;
            const double fd   = targets[t].dopplerFreq;
            const double phi0 = targets[t].initPhase;
            const double phase = 2.0 * M_PI * fd * static_cast<double>(n) + phi0;
            sample += Complex(amp * std::cos(phase), amp * std::sin(phase));
        }

        burst[static_cast<size_t>(n)] = sample;
    }
    return burst;
}

/**
 * Добавляет НИП (одиночный прямоугольный импульс) в позицию m пачки.
 * Модель НИП: x[m] += A_nip · exp(j·φ_nip)
 */
void addNip(CVector& burst, int m, double amplitude, double phase)
{
    if (m < 0 || m >= static_cast<int>(burst.size())) return;
    burst[static_cast<size_t>(m)] +=
        Complex(amplitude * std::cos(phase), amplitude * std::sin(phase));
}

/**
 * Вывод статистики по пачке.
 */
void printBurstInfo(const std::string& label, const CVector& burst)
{
    double sumMag = 0.0, maxMag = 0.0;
    for (size_t i = 0; i < burst.size(); ++i) {
        double m = std::abs(burst[i]);
        sumMag += m;
        if (m > maxMag) maxMag = m;
    }
    double mean = burst.empty() ? 0.0 : sumMag / static_cast<double>(burst.size());
    std::cout << "    " << label
              << ": N=" << burst.size()
              << ", mean|x|=" << std::fixed << std::setprecision(4) << mean
              << ", max|x|="  << std::fixed << std::setprecision(4) << maxMag << "\n";
}

/**
 * Сохраняет пару (clean, noisy) в файлы формата  <base>_clean.csv / <base>_noisy.csv.
 */
void savePair(const CVector& clean,
              const CVector& noisy,
              const std::string& outDir,
              int idx)
{
    std::ostringstream ss;
    ss << outDir << "/burst_" << std::setw(2) << std::setfill('0') << idx;
    const std::string base = ss.str();

    DopplerNipFilter::saveToCSV(clean, base + "_clean.csv");
    DopplerNipFilter::saveToCSV(noisy, base + "_noisy.csv");

    std::cout << "  Сохранён сценарий " << idx
              << ": " << base << "_[clean|noisy].csv\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// main
// ═════════════════════════════════════════════════════════════════════════════

int main()
{
    const std::string outDir = std::string(ROOT_PATH) + "/data/radar";
    // Создаём директорию (POSIX)
    mkdir(outDir.c_str(), 0755);

    std::cout << "Генерация тестовых РЛС данных → " << outDir << "\n\n";

    std::mt19937 rng(12345u);

    // Общие параметры пачки
    const int    N        = 64;    // число зондирующих импульсов (степень двойки)
    const double noiseStd = 0.05;  // СКО шума приёмника

    // ════════════════════════════════════════════════════════════════════════
    // Сценарий 0: 1 цель (f_d=0.1), НИП в импульсе 4, A_nip=10×A_цель
    // ════════════════════════════════════════════════════════════════════════
    {
        const int    sc      = 0;
        const double fd      = 0.10;
        const double amp     = 1.0;
        const int    nipIdx  = 4;
        const double nipAmp  = 10.0 * amp;
        const double nipPhi  = 0.3;

        std::cout << "Сценарий " << sc << ": 1 цель (f_d=" << fd
                  << "), НИП[" << nipIdx << "], A_nip=" << nipAmp << "\n";

        std::vector<Target> targets = { {amp, fd, 0.0} };
        CVector clean = generateBurst(N, targets, noiseStd, rng);
        CVector noisy = clean;
        addNip(noisy, nipIdx, nipAmp, nipPhi);

        printBurstInfo("clean", clean);
        printBurstInfo("noisy", noisy);
        savePair(clean, noisy, outDir, sc);
        std::cout << "\n";
    }

    // ════════════════════════════════════════════════════════════════════════
    // Сценарий 1: 1 цель (f_d=0.25), НИП в импульсе 12, A_nip=20×A_цель
    // ════════════════════════════════════════════════════════════════════════
    {
        const int    sc      = 1;
        const double fd      = 0.25;
        const double amp     = 1.0;
        const int    nipIdx  = 12;
        const double nipAmp  = 20.0 * amp;
        const double nipPhi  = 1.2;

        std::cout << "Сценарий " << sc << ": 1 цель (f_d=" << fd
                  << "), НИП[" << nipIdx << "], A_nip=" << nipAmp << "\n";

        std::vector<Target> targets = { {amp, fd, 0.0} };
        CVector clean = generateBurst(N, targets, noiseStd, rng);
        CVector noisy = clean;
        addNip(noisy, nipIdx, nipAmp, nipPhi);

        printBurstInfo("clean", clean);
        printBurstInfo("noisy", noisy);
        savePair(clean, noisy, outDir, sc);
        std::cout << "\n";
    }

    // ════════════════════════════════════════════════════════════════════════
    // Сценарий 2: нет цели (только шум), НИП в импульсе 7
    // ════════════════════════════════════════════════════════════════════════
    {
        const int    sc      = 2;
        const int    nipIdx  = 7;
        const double nipAmp  = 5.0;
        const double nipPhi  = -0.7;

        std::cout << "Сценарий " << sc
                  << ": только шум, НИП[" << nipIdx << "], A_nip=" << nipAmp << "\n";

        std::vector<Target> targets;  // нет целей
        CVector clean = generateBurst(N, targets, noiseStd, rng);
        CVector noisy = clean;
        addNip(noisy, nipIdx, nipAmp, nipPhi);

        printBurstInfo("clean", clean);
        printBurstInfo("noisy", noisy);
        savePair(clean, noisy, outDir, sc);
        std::cout << "\n";
    }

    // ════════════════════════════════════════════════════════════════════════
    // Сценарий 3: 2 цели (f_d=0.1 и f_d=0.3), НИП в импульсе 0
    // ════════════════════════════════════════════════════════════════════════
    {
        const int    sc      = 3;
        const int    nipIdx  = 0;
        const double nipAmp  = 15.0;
        const double nipPhi  = 2.1;

        std::cout << "Сценарий " << sc
                  << ": 2 цели (f_d=0.1, 0.3), НИП[" << nipIdx
                  << "], A_nip=" << nipAmp << "\n";

        std::vector<Target> targets = { {1.0, 0.10, 0.0}, {0.6, 0.30, 1.0} };
        CVector clean = generateBurst(N, targets, noiseStd, rng);
        CVector noisy = clean;
        addNip(noisy, nipIdx, nipAmp, nipPhi);

        printBurstInfo("clean", clean);
        printBurstInfo("noisy", noisy);
        savePair(clean, noisy, outDir, sc);
        std::cout << "\n";
    }

    // ════════════════════════════════════════════════════════════════════════
    // Сценарий 4: 1 цель (f_d=0.05), НИП в ПОСЛЕДНЕМ импульсе (m=N-1)
    // ════════════════════════════════════════════════════════════════════════
    {
        const int    sc      = 4;
        const double fd      = 0.05;
        const double amp     = 1.0;
        const int    nipIdx  = N - 1;
        const double nipAmp  = 25.0;
        const double nipPhi  = -1.5;

        std::cout << "Сценарий " << sc << ": 1 цель (f_d=" << fd
                  << "), НИП[N-1=" << nipIdx << "], A_nip=" << nipAmp << "\n";

        std::vector<Target> targets = { {amp, fd, 0.5} };
        CVector clean = generateBurst(N, targets, noiseStd, rng);
        CVector noisy = clean;
        addNip(noisy, nipIdx, nipAmp, nipPhi);

        printBurstInfo("clean", clean);
        printBurstInfo("noisy", noisy);
        savePair(clean, noisy, outDir, sc);
        std::cout << "\n";
    }

    // ════════════════════════════════════════════════════════════════════════
    // Сценарий 5: 1 цель, НИП НЕ ДОБАВЛЯЕТСЯ (контроль — CV должен быть большим)
    // ════════════════════════════════════════════════════════════════════════
    {
        const int    sc  = 5;
        const double fd  = 0.15;
        const double amp = 1.0;

        std::cout << "Сценарий " << sc
                  << ": 1 цель (f_d=" << fd << "), без НИП (контроль)\n";

        std::vector<Target> targets = { {amp, fd, 0.0} };
        CVector clean = generateBurst(N, targets, noiseStd, rng);
        CVector noisy = clean;  // НИП нет → noisy == clean

        printBurstInfo("clean", clean);
        printBurstInfo("noisy", noisy);
        savePair(clean, noisy, outDir, sc);
        std::cout << "\n";
    }

    std::cout << "════════════════════════════════════════════════\n";
    std::cout << "Генерация завершена.\n";
    std::cout << "  N=" << N << " импульсов, σ_шума=" << noiseStd << "\n";
    std::cout << "  Формат: Re,Im (строка = один зондирующий импульс)\n";

    return 0;
}
