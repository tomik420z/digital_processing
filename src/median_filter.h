#ifndef MEDIAN_FILTER_H
#define MEDIAN_FILTER_H

#include "signal_processor.h"
#include <cstddef>

/**
 * Медианный фильтр для подавления импульсных помех
 */
class MedianFilter : public SignalProcessor {
private:
    size_t windowSize_;  // Размер окна фильтрации

public:
    /**
     * Конструктор
     * @param windowSize Размер окна фильтрации (должен быть нечетным)
     */
    explicit MedianFilter(size_t windowSize = 5);

    /**
     * Применить медианный фильтр к сигналу
     * @param input Входной сигнал
     * @return Отфильтрованный сигнал
     */
    Signal process(const Signal& input) override;

    /**
     * Получить имя алгоритма
     */
    std::string getName() const override;

    /**
     * Установить размер окна
     * @param windowSize Новый размер окна (должен быть нечетным)
     */
    void setWindowSize(size_t windowSize);

    /**
     * Получить текущий размер окна
     */
    size_t getWindowSize() const;

private:
    /**
     * Вычислить медиану в скользящем окне
     * @param input Входной сигнал
     * @param index Центральный индекс окна
     * @return Медиана окна
     */
    double computeWindowMedian(const Signal& input, size_t index) const;

    bool IsValidWindowSize(size_t windowSize) const;
};

#endif // MEDIAN_FILTER_H
