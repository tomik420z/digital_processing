#ifndef DOPPLER_NIP_FILTER_H
#define DOPPLER_NIP_FILTER_H

/**
 * Алгоритм подавления несинхронных импульсных помех (НИП)
 * на основе доплеровских фазовых фильтров.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * Теоретическое обоснование
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Пусть x[n], n=0..N-1 — комплексные квадратурные отсчёты одного
 * дискрета дальности за N зондирующих импульсов пачки:
 *
 *   x[n] = s[n] + c[n] + η[n]
 *
 * где:
 *   s[n] — эхо-сигнал от цели (в общем случае, c[n] = 0 при отсутствии цели)
 *   c[n] — НИП (≠ 0 только при n = m, т.е. в одном импульсе)
 *   η[n] — аддитивный белый шум приёмника
 *
 * Доплеровский банк фильтров (ДПФ по «медленному времени»):
 *
 *   Y[k] = (1/N) · Σ_{n=0}^{N-1} x[n] · exp(-j·2π·k·n/N),   k=0..N-1
 *
 * Если НИП присутствует только в импульсе m:
 *
 *   Y_НИП[k] = (A/N) · exp(-j·2π·k·m/N)
 *
 * Ключевое свойство: |Y_НИП[k]| = A/N = const для всех k.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * Алгоритм
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Шаг 1. Вычислить ДПФ: Y = DFT{x}
 *
 * Шаг 2. Обнаружение НИП.
 *   Оцениваем разброс |Y[k]|. Если σ(|Y|)/μ(|Y|) < порога нет — НИП не обнаружена.
 *   Дополнительно: если min(|Y[k]|) > порог обнаружения → НИП присутствует.
 *
 * Шаг 3. Оценка параметров НИП.
 *   а) Амплитуда: Â = N · min_k(|Y[k]|)  (НИП равномерно распределена по всем бинам)
 *   б) Индекс m определяется через линейную фазу:
 *      Δφ[k] = arg(Y[k+1]) − arg(Y[k]) ≈ −2π·m/N
 *      m̂ = round( −N · Δφ̄ / (2π) )
 *
 * Шаг 4. Компенсация НИП.
 *   Ŷ[k] = Y[k] − (Â/N) · exp(−j·2π·k·m̂/N),   k=0..N-1
 *
 * Шаг 5. Восстановление: x̂ = IDFT{Ŷ}
 *
 * ─────────────────────────────────────────────────────────────────────────
 * Формат данных
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Входной / выходной тип: CVector = vector<complex<double>>
 *   - Размер N = число зондирующих импульсов в пачке (рекомендуется 2^k)
 *   - x[n] = I[n] + j·Q[n]  (квадратурные компоненты)
 *
 * Для совместимости с SignalProcessor: вещественная и мнимая части
 * отдельно доступны через методы getRealPart() / getImagPart().
 *
 * ─────────────────────────────────────────────────────────────────────────
 * CSV формат входных файлов (data/radar)
 * ─────────────────────────────────────────────────────────────────────────
 *   Строки: n=0..N-1 (один импульс на строку)
 *   Столбцы: Re,Im
 *   Пример:
 *     0.1234,0.5678
 *     0.2345,0.6789
 *     15.789,-0.123      (импульс m с НИП)
 *     ...
 */

#include "signal_processor.h"
#include "utils/fft.h"

#include <complex>
#include <vector>
#include <string>
#include <cstddef>

/// Комплексный сигнал (вектор I+jQ)
using ComplexSignal = CVector;  // CVector = vector<complex<double>> из fft.h

/**
 * Структура результата обнаружения НИП.
 */
struct NipDetectionResult {
    bool    detected;       ///< true — НИП обнаружена
    int     pulseIndex;     ///< Оценённый индекс поражённого импульса m̂ (-1 если не обнаружена)
    double  amplitude;      ///< Оценённая амплитуда НИП Â (0 если не обнаружена)
    double  phaseRad;       ///< Начальная фаза НИП φ₀ (радианы)
    double  detectionMetric;///< Значение метрики обнаружения (cv = σ/μ модулей спектра)

    NipDetectionResult()
        : detected(false), pulseIndex(-1), amplitude(0.0),
          phaseRad(0.0), detectionMetric(0.0) {}
};

/**
 * Алгоритм подавления НИП на основе доплеровских фазовых фильтров.
 *
 * Применяется к одному дискрету дальности — вектору из N комплексных
 * отсчётов (по числу зондирующих импульсов в пачке).
 */
class DopplerNipFilter {
public:
    /**
     * Конструктор
     *
     * @param detectionThreshold   Порог обнаружения по коэффициенту вариации
     *                             CV = σ(|Y|)/μ(|Y|). При CV < threshold НИП обнаружена.
     *                             Типичное значение: 0.5 (50% разброса модулей).
     *                             Чем меньше — тем строже критерий.
     *
     * @param minRelativeAmplitude Минимальная относительная амплитуда НИП
     *                             (доля от максимума спектра) для подтверждения.
     *                             Типичное значение: 0.05 (5%).
     *
     * @param phaseAveragingWindow Окно усреднения фазовых разностей для оценки m.
     *                             0 = использовать все N-1 разностей.
     */
    explicit DopplerNipFilter(double detectionThreshold   = 0.50,
                              double minRelativeAmplitude = 0.05,
                              int    phaseAveragingWindow = 0);

    // ── Основная обработка ────────────────────────────────────────────────

    /**
     * Применить алгоритм к одному дискрету дальности.
     *
     * @param burstSamples  Входной вектор N комплексных отсчётов (I+jQ),
     *                      один отсчёт на зондирующий импульс.
     * @return              Скомпенсированный вектор той же длины N.
     *
     * После вызова доступны:
     *   - getLastDetection()      — результат обнаружения
     *   - getSpectrumBefore()     — |Y[k]| в дБ до компенсации
     *   - getSpectrumAfter()      — |Ŷ[k]| в дБ после компенсации
     */
    ComplexSignal process(const ComplexSignal& burstSamples);

    /**
     * Перегрузка: вещественные I и Q компоненты раздельно.
     */
    ComplexSignal process(const SignalProcessor::Signal& iChannel,
                          const SignalProcessor::Signal& qChannel);

    // ── Доступ к результатам ──────────────────────────────────────────────

    /** Результат последнего обнаружения */
    const NipDetectionResult& getLastDetection() const { return lastDetection_; }

    /** Доплеровский спектр (модули в дБ) ДО компенсации.
     *  Размер N. Индекс соответствует доплеровскому каналу k. */
    const SignalProcessor::Signal& getSpectrumBefore() const { return spectrumBefore_; }

    /** Доплеровский спектр (модули в дБ) ПОСЛЕ компенсации. */
    const SignalProcessor::Signal& getSpectrumAfter()  const { return spectrumAfter_;  }

    /** Разность спектров (до − после) в дБ для отображения в GUI. */
    SignalProcessor::Signal getSpectrumDiff() const;

    /** Имя алгоритма */
    std::string getName() const;

    // ── Утилиты ──────────────────────────────────────────────────────────

    /**
     * Извлечь вещественную часть (I-канал) из комплексного вектора.
     */
    static SignalProcessor::Signal getRealPart(const ComplexSignal& cs);

    /**
     * Извлечь мнимую часть (Q-канал) из комплексного вектора.
     */
    static SignalProcessor::Signal getImagPart(const ComplexSignal& cs);

    /**
     * Вычислить модуль (огибающую) комплексного сигнала.
     */
    static SignalProcessor::Signal getMagnitude(const ComplexSignal& cs);

    /**
     * Загрузить комплексный сигнал из CSV (формат Re,Im на строку).
     */
    static ComplexSignal loadFromCSV(const std::string& filename);

    /**
     * Сохранить комплексный сигнал в CSV (формат Re,Im на строку).
     */
    static void saveToCSV(const ComplexSignal& signal, const std::string& filename);

    /**
     * Установить параметры алгоритма.
     */
    void setParameters(double detectionThreshold,
                       double minRelativeAmplitude,
                       int    phaseAveragingWindow = 0);

private:
    double detectionThreshold_;    ///< Порог CV для обнаружения
    double minRelativeAmplitude_;  ///< Минимальная относительная амплитуда
    int    phaseAveragingWindow_;  ///< Окно усреднения фазовых разностей

    NipDetectionResult          lastDetection_;  ///< Кэш результата обнаружения
    SignalProcessor::Signal     spectrumBefore_; ///< Спектр до компенсации (дБ)
    SignalProcessor::Signal     spectrumAfter_;  ///< Спектр после компенсации (дБ)

    /**
     * Вычислить доплеровский спектр (ДПФ от N отсчётов).
     * Возвращает нормированные коэффициенты Y[k] = (1/N)·DFT{x}.
     */
    static ComplexSignal computeDFT(const ComplexSignal& x);

    /**
     * Обратное ДПФ: IDFT{Y} → вектор длины N.
     */
    static ComplexSignal computeIDFT(const ComplexSignal& Y);

    /**
     * Обнаружить НИП в доплеровском спектре Y.
     * Заполняет поля lastDetection_.
     */
    NipDetectionResult detectNip(const ComplexSignal& Y) const;

    /**
     * Оценить индекс поражённого импульса m по фазовым разностям спектра.
     * @param Y      Доплеровский спектр (нормированный)
     * @param N      Размер пачки
     * @return       Оценённый индекс m (0..N-1)
     */
    int estimatePulseIndex(const ComplexSignal& Y, int N) const;

    /**
     * Компенсировать НИП из доплеровского спектра.
     * @param Y      Входной спектр (модифицируется на месте)
     * @param det    Параметры обнаруженной НИП
     * @param N      Размер пачки
     */
    static void compensateNip(ComplexSignal& Y,
                              const NipDetectionResult& det,
                              int N);

    /**
     * Преобразовать вектор модулей в дБ (20·log10(|Y[k]| + eps)).
     */
    static SignalProcessor::Signal toDecibels(const ComplexSignal& Y);
};

#endif // DOPPLER_NIP_FILTER_H
