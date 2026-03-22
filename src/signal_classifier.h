#ifndef SIGNAL_CLASSIFIER_H
#define SIGNAL_CLASSIFIER_H

#include "signal_processor.h"
#include <string>

/**
 * Классификатор типа входного сигнала по набору статистических признаков.
 *
 * Алгоритм работает без FFT: использует только временны́е характеристики,
 * которые вычисляются за O(N) или O(N·w).
 *
 * Распознаваемые классы:
 *   SINE       — синусоидальный (гладкий, периодический)
 *   SQUARE     — квадратный меандр (резкие переходы, 2 уровня)
 *   TRIANGLE   — треугольный / пилообразный (линейные участки)
 *   ECHO       — разреженный импульсный (эхо-сигнал: большинство отсчётов ≈ 0)
 *   NOISY      — сигнал с выраженными импульсными помехами (высокий куртозис)
 *   UNKNOWN    — не удалось определить
 *
 * Признаки (Features):
 *   kurtosis        — 4-й центральный момент / σ⁴ (высокий → импульс)
 *   crestFactor     — max|x| / RMS (высокий → острые пики)
 *   zeroCrossingRate— частота смены знака (характеристика периодичности)
 *   sparsity        — доля отсчётов |x[n]| < ε·max|x|
 *   smoothness      — средний |x[n]-x[n-1]| (малая → гладкий сигнал)
 *   localVarRatio   — max(localVar)/mean(localVar) (высокий → нестационарный)
 *   rangeRatio      — (max-min)/2/RMS (≈1 для синуса, >1 для меандра)
 */
class SignalClassifier {
public:
    using Signal = SignalProcessor::Signal;

    /**
     * Тип сигнала, распознанный классификатором
     */
    enum class SignalType {
        SINE,       ///< Гладкий синусоидальный сигнал
        SQUARE,     ///< Меандр / квадратный сигнал
        TRIANGLE,   ///< Треугольный или пилообразный сигнал
        ECHO,       ///< Разреженный импульсный (эхо-радиолокационный)
        NOISY,      ///< Сигнал с выраженными импульсными помехами
        UNKNOWN     ///< Не удалось определить
    };

    /**
     * Набор извлечённых признаков сигнала
     */
    struct Features {
        double kurtosis         = 0.0; ///< Куртозис (острота пиков распределения)
        double crestFactor      = 0.0; ///< Пик-фактор: max|x|/RMS
        double zeroCrossingRate = 0.0; ///< Доля отсчётов со сменой знака
        double sparsity         = 0.0; ///< Доля «почти нулевых» отсчётов
        double smoothness       = 0.0; ///< Средний |Δx| (нормированный)
        double localVarRatio    = 0.0; ///< max(локVar)/mean(локВар)
        double rangeRatio       = 0.0; ///< (max-min)/2 / RMS
    };

    /**
     * Конструктор
     * @param localWindow  Размер окна для вычисления локальной дисперсии (нечётное)
     * @param sparseEps    Порог разреженности: отсчёт считается «нулём» при |x|<sparseEps*max
     */
    explicit SignalClassifier(size_t localWindow = 31, double sparseEps = 0.05);

    /**
     * Извлечь признаки из сигнала
     * @param signal Входной сигнал
     * @return Структура Features
     */
    Features extractFeatures(const Signal& signal) const;

    /**
     * Классифицировать сигнал по его признакам
     * @param features Признаки (результат extractFeatures)
     * @return Тип сигнала
     */
    SignalType classify(const Features& features) const;

    /**
     * Удобный метод: извлечь признаки и сразу классифицировать
     * @param signal Входной сигнал
     * @return Тип сигнала
     */
    SignalType classifySignal(const Signal& signal) const;

    /**
     * Получить строковое представление типа сигнала
     */
    static std::string typeToString(SignalType type);

private:
    size_t localWindow_; ///< Окно локальной дисперсии
    double sparseEps_;   ///< Порог разреженности

    /// Вычислить среднее
    static double mean(const Signal& x);
    /// Вычислить дисперсию
    static double variance(const Signal& x, double m);
    /// Вычислить RMS
    static double rms(const Signal& x);
};

#endif // SIGNAL_CLASSIFIER_H
