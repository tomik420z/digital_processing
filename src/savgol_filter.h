#ifndef SAVGOL_FILTER_H
#define SAVGOL_FILTER_H

#include "signal_processor.h"
#include <vector>

/**
 * Фильтр Савицкого-Голая для сглаживания сигналов
 */
class SavgolFilter : public SignalProcessor {
private:
    size_t windowSize_;     // Размер окна фильтрации (должен быть нечетным)
    size_t polyOrder_;      // Порядок аппроксимирующего полинома
    std::vector<double> coefficients_; // Коэффициенты фильтра

public:
    /**
     * Конструктор
     * @param windowSize Размер окна фильтрации (должен быть нечетным)
     * @param polyOrder Порядок полинома (должен быть < windowSize)
     */
    SavgolFilter(size_t windowSize = 11, size_t polyOrder = 3);

    /**
     * Применить фильтр Савицкого-Голая к сигналу
     * @param input Входной сигнал
     * @return Отфильтрованный сигнал
     */
    Signal process(const Signal& input) override;

    /**
     * Получить имя алгоритма
     */
    std::string getName() const override;

    /**
     * Установить параметры фильтра
     * @param windowSize Новый размер окна
     * @param polyOrder Новый порядок полинома
     */
    void setParameters(size_t windowSize, size_t polyOrder);

    /**
     * Получить текущий размер окна
     */
    size_t getWindowSize() const { return windowSize_; }

    /**
     * Получить текущий порядок полинома
     */
    size_t getPolyOrder() const { return polyOrder_; }

private:
    /**
     * Вычислить коэффициенты фильтра Савицкого-Голая
     * Использует метод наименьших квадратов для аппроксимации полиномом
     */
    void calculateCoefficients();

    /**
     * Решение системы линейных уравнений методом Гаусса
     * @param matrix Матрица коэффициентов (будет изменена)
     * @param rhs Правая часть уравнения (будет изменена)
     * @return Решение системы
     */
    std::vector<double> gaussElimination(std::vector<std::vector<double>>& matrix,
                                        std::vector<double>& rhs) const;

    /**
     * Применить фильтр в точке с заданным индексом
     * @param input Входной сигнал
     * @param centerIndex Индекс центральной точки окна
     * @return Отфильтрованное значение
     */
    double applyFilter(const Signal& input, size_t centerIndex) const;

    /**
     * Обработка краевых эффектов - отражение сигнала
     * @param input Входной сигнал
     * @param index Требуемый индекс (может быть вне границ)
     * @return Значение с учетом отражения
     */
    double getReflectedValue(const Signal& input, int index) const;
};

#endif // SAVGOL_FILTER_H