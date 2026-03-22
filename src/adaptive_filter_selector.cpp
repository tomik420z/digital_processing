#include "adaptive_filter_selector.h"

#include "median_filter.h"
#include "wiener_filter.h"
#include "robust_wiener_filter.h"
#include "savgol_filter.h"
#include "kalman_filter.h"
#include "morphological_filter.h"
#include "spectral_subtraction_filter.h"

#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Конструктор
// ─────────────────────────────────────────────────────────────────────────────

AdaptiveFilterSelector::AdaptiveFilterSelector(size_t localWindow, double sparseEps)
    : classifier_(localWindow, sparseEps)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Выбор фильтра по типу сигнала
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<SignalProcessor>
AdaptiveFilterSelector::selectFilter(const Signal& signal,
                                      SignalClassifier::SignalType& detectedType) const
{
    detectedType = classifier_.classifySignal(signal);

    switch (detectedType) {

        // ── Синусоидальный ────────────────────────────────────────────────────
        // Гладкий, стационарный. Классический фильтр Винера оптимален для
        // минимизации MSE при гауссовом шуме на периодическом сигнале.
        case SignalClassifier::SignalType::SINE:
            return std::make_unique<WienerFilter>(
                /*filterOrder=*/   16,
                /*desiredWindow=*/  9,
                /*regularization=*/ 1e-4);

        // ── Меандр ────────────────────────────────────────────────────────────
        // Резкие переходы. Медианный фильтр сохраняет фронты и не сглаживает
        // скачки (в отличие от Винера, который «размывает» острые края).
        case SignalClassifier::SignalType::SQUARE:
            return std::make_unique<MedianFilter>(/*windowSize=*/5);

        // ── Треугольный / пилообразный ────────────────────────────────────────
        // Кусочно-линейный сигнал. SavGol poly=1 (локальная линейная аппроксимация)
        // точно восстанавливает линейные участки и сохраняет изломы.
        case SignalClassifier::SignalType::TRIANGLE:
            return std::make_unique<SavgolFilter>(/*windowSize=*/11, /*polyOrder=*/1);

        // ── Эхо-импульсный ────────────────────────────────────────────────────
        // Разреженный сигнал (большинство отсчётов ≈ 0, редкие пики).
        // Фильтр Калмана с малым шумом процесса хорошо отслеживает
        // динамику редких пиков и подавляет шум в «тишине» между ними.
        case SignalClassifier::SignalType::ECHO:
            return std::make_unique<KalmanFilter>(
                /*processNoise=*/     0.01,
                /*measurementNoise=*/ 0.5,
                /*deltaT=*/           1.0);

        // ── Сигнал с несинхронными импульсными помехами ───────────────────────
        // SpectralSubtractionFilter: FFT-based подавление широкополосных
        // импульсных выбросов через вычитание оценки шумового спектра.
        // Импульсные помехи дают равномерный «пьедестал» в спектре,
        // который эффективно устраняется методом Болла (1979).
        case SignalClassifier::SignalType::NOISY:
            return std::make_unique<SpectralSubtractionFilter>(
                /*frameSize=*/        256,
                /*hopSize=*/           64,
                /*noiseFrames=*/        4,
                /*subtractionFactor=*/  2.5,
                /*spectralFloor=*/      0.002,
                /*noiseUpdateRate=*/    0.1,
                /*noiseThreshold=*/     1.5);

        // ── Неизвестный тип ───────────────────────────────────────────────────
        // Медианный фильтр — универсальный «безопасный» выбор:
        // хорошо подавляет как гауссов, так и импульсный шум.
        default:
            return std::make_unique<MedianFilter>(/*windowSize=*/7);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Удобный метод: классифицировать → выбрать → применить
// ─────────────────────────────────────────────────────────────────────────────

AdaptiveFilterSelector::Signal
AdaptiveFilterSelector::processAuto(const Signal& signal,
                                     SignalClassifier::SignalType& detectedType,
                                     std::string& filterName) const
{
    auto filter = selectFilter(signal, detectedType);
    filterName  = filter->getName();
    return filter->process(signal);
}

// ─────────────────────────────────────────────────────────────────────────────
// Описание правила выбора
// ─────────────────────────────────────────────────────────────────────────────

std::string AdaptiveFilterSelector::getSelectionReason(SignalClassifier::SignalType type)
{
    switch (type) {
        case SignalClassifier::SignalType::SINE:
            return "SINE → WienerFilter(16,9,1e-4): оптимален для гауссова шума на гладком сигнале";
        case SignalClassifier::SignalType::SQUARE:
            return "SQUARE → MedianFilter(5): сохраняет резкие фронты меандра";
        case SignalClassifier::SignalType::TRIANGLE:
            return "TRIANGLE → SavgolFilter(11,1): кусочно-линейная аппроксимация сохраняет изломы";
        case SignalClassifier::SignalType::ECHO:
            return "ECHO → KalmanFilter(0.01,0.5): отслеживает редкие пики, подавляет шум в паузах";
        case SignalClassifier::SignalType::NOISY:
            return "NOISY → SpectralSubtractionFilter(256,64,4,2.5,0.002): FFT-вычитание шумового спектра (Boll 1979)";
        default:
            return "UNKNOWN → MedianFilter(7): универсальный безопасный выбор";
    }
}
