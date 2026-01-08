#ifndef OUTLIER_DETECTION_H
#define OUTLIER_DETECTION_H

#include "signal_processor.h"

/**
 * Алгоритм обнаружения и замещения импульсных помех (выбросов)
 */
class OutlierDetection : public SignalProcessor {
public:
    /**
     * Методы обнаружения выбросов
     */
    enum class DetectionMethod {
        MAD_BASED,          // На основе медианного абсолютного отклонения
        STATISTICAL,        // Статистический метод (z-score)
        ADAPTIVE_THRESHOLD  // Адаптивный порог
    };

    /**
     * Методы интерполяции для замещения выбросов
     */
    enum class InterpolationMethod {
        LINEAR,             // Линейная интерполяция
        SPLINE,             // Сплайн-интерполяция (кубическая)
        MEDIAN_BASED,       // На основе медианы соседних значений
        AUTOREGRESSIVE      // Авторегрессионная экстраполяция
    };

private:
    DetectionMethod detectionMethod_;
    InterpolationMethod interpolationMethod_;
    double threshold_;          // Порог обнаружения
    size_t windowSize_;         // Размер окна для анализа
    size_t arOrder_;           // Порядок авторегрессионной модели

public:
    /**
     * Конструктор
     * @param detectionMethod Метод обнаружения выбросов
     * @param interpolationMethod Метод интерполяции
     * @param threshold Порог обнаружения (в единицах MAD или стандартных отклонениях)
     * @param windowSize Размер окна для анализа
     */
    OutlierDetection(DetectionMethod detectionMethod = DetectionMethod::MAD_BASED,
                     InterpolationMethod interpolationMethod = InterpolationMethod::LINEAR,
                     double threshold = 3.0,
                     size_t windowSize = 11);

    /**
     * Применить алгоритм обнаружения и замещения выбросов
     * @param input Входной сигнал
     * @return Отфильтрованный сигнал
     */
    Signal process(const Signal& input) override;

    /**
     * Получить имя алгоритма
     */
    std::string getName() const override;

    /**
     * Установить параметры алгоритма
     * @param detectionMethod Метод обнаружения
     * @param interpolationMethod Метод интерполяции
     * @param threshold Порог обнаружения
     * @param windowSize Размер окна
     */
    void setParameters(DetectionMethod detectionMethod,
                      InterpolationMethod interpolationMethod,
                      double threshold,
                      size_t windowSize);

    /**
     * Обнаружить выбросы в сигнале
     * @param input Входной сигнал
     * @return Вектор булевых значений (true = выброс)
     */
    std::vector<bool> detectOutliers(const Signal& input) const;

private:
    /**
     * Обнаружение выбросов на основе MAD
     * @param input Входной сигнал
     * @return Индексы выбросов
     */
    std::vector<bool> detectMADBased(const Signal& input) const;

    /**
     * Статистическое обнаружение выбросов
     * @param input Входной сигнал
     * @return Индексы выбросов
     */
    std::vector<bool> detectStatistical(const Signal& input) const;

    /**
     * Обнаружение с адаптивным порогом
     * @param input Входной сигнал
     * @return Индексы выбросов
     */
    std::vector<bool> detectAdaptiveThreshold(const Signal& input) const;

    /**
     * Линейная интерполяция выброса
     * @param input Исходный сигнал
     * @param outliers Маска выбросов
     * @return Интерполированный сигнал
     */
    Signal interpolateLinear(const Signal& input, const std::vector<bool>& outliers) const;

    /**
     * Интерполяция на основе медианы
     * @param input Исходный сигнал
     * @param outliers Маска выбросов
     * @return Интерполированный сигнал
     */
    Signal interpolateMedian(const Signal& input, const std::vector<bool>& outliers) const;

    /**
     * Авторегрессионная интерполяция
     * @param input Исходный сигнал
     * @param outliers Маска выбросов
     * @return Интерполированный сигнал
     */
    Signal interpolateAutoregressive(const Signal& input, const std::vector<bool>& outliers) const;

    /**
     * Найти ближайшие нормальные точки
     * @param outliers Маска выбросов
     * @param index Индекс выброса
     * @return Пара индексов (левая, правая нормальная точка)
     */
    std::pair<int, int> findNearestNormalPoints(const std::vector<bool>& outliers, size_t index) const;

    /**
     * Получить строковое представление метода обнаружения
     */
    static std::string detectionMethodToString(DetectionMethod method);

    /**
     * Получить строковое представление метода интерполяции
     */
    static std::string interpolationMethodToString(InterpolationMethod method);
};

#endif // OUTLIER_DETECTION_H