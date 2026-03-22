#ifndef ADAPTIVE_FILTER_SELECTOR_H
#define ADAPTIVE_FILTER_SELECTOR_H

#include "signal_processor.h"
#include "signal_classifier.h"

#include <memory>
#include <string>

/**
 * Адаптивный выбор фильтра на основе автоматической классификации сигнала.
 *
 * Логика маппинга «тип сигнала → оптимальный фильтр»:
 *
 *   SINE      → WienerFilter (порядок 16, окно 9)
 *               Синус — гладкий, стационарный, гауссов шум → классический Винер оптимален.
 *
 *   SQUARE    → MedianFilter (окно 5) → SavgolFilter (окно 11, poly 1)
 *               Меандр имеет резкие переходы. Медиана не сглаживает фронты.
 *               SavGol с poly=1 (кусочно-линейный) тоже хорошо сохраняет скачки.
 *               Используем SavgolFilter, т.к. он один объект (MedianFilter тоже вариант).
 *
 *   TRIANGLE  → SavgolFilter (окно 11, poly 1)
 *               Треугольник — кусочно-линейный. SavGol poly=1 точно аппроксимирует
 *               линейные участки и сохраняет изломы.
 *
 *   ECHO      → KalmanFilter (processNoise=0.01, measureNoise=0.5)
 *               Разреженный импульсный сигнал. Kalman с малым шумом процесса
 *               хорошо отслеживает редкие пики и подавляет шум между ними.
 *
 *   NOISY     → RobustWienerFilter (порядок 10, медиана, OutlierDetection)
 *               Сигнал с несинхронными импульсными помехами → явный кандидат
 *               для робастного Винера с предварительной очисткой выбросов.
 *
 *   UNKNOWN   → MedianFilter (окно 7) — универсальный «безопасный» выбор.
 */
class AdaptiveFilterSelector {
public:
    using Signal = SignalProcessor::Signal;

    /**
     * Конструктор
     * @param localWindow   Окно локальной дисперсии для классификатора
     * @param sparseEps     Порог разреженности для классификатора
     */
    explicit AdaptiveFilterSelector(size_t localWindow = 31, double sparseEps = 0.05);

    /**
     * Автоматически классифицировать сигнал и создать оптимальный фильтр.
     *
     * @param signal      Входной сигнал (используется только для классификации)
     * @param detectedType  Выходной параметр — определённый тип сигнала
     * @return unique_ptr<SignalProcessor> — созданный фильтр, готовый к вызову process()
     */
    std::unique_ptr<SignalProcessor> selectFilter(
        const Signal& signal,
        SignalClassifier::SignalType& detectedType) const;

    /**
     * Классифицировать сигнал, выбрать и сразу применить оптимальный фильтр.
     *
     * @param signal        Входной (зашумлённый) сигнал
     * @param detectedType  Выходной параметр — тип сигнала
     * @param filterName    Выходной параметр — имя выбранного фильтра
     * @return Отфильтрованный сигнал
     */
    Signal processAuto(const Signal& signal,
                       SignalClassifier::SignalType& detectedType,
                       std::string& filterName) const;

    /**
     * Получить описание правила выбора для типа сигнала
     */
    static std::string getSelectionReason(SignalClassifier::SignalType type);

private:
    SignalClassifier classifier_; ///< Внутренний классификатор
};

#endif // ADAPTIVE_FILTER_SELECTOR_H
