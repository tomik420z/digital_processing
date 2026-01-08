#ifndef WIENER_FILTER_H
#define WIENER_FILTER_H

#include "signal_processor.h"

/**
 * Адаптивный фильтр Винера для подавления помех
 */
class WienerFilter : public SignalProcessor {
private:
    size_t filterOrder_;        // Порядок фильтра
    double mu_;                 // Шаг адаптации
    double lambda_;             // Коэффициент забывания
    std::vector<double> weights_; // Веса фильтра

public:
    /**
     * Конструктор
     * @param filterOrder Порядок фильтра
     * @param mu Шаг адаптации (0 < mu < 1)
     * @param lambda Коэффициент забывания (0 < lambda <= 1)
     */
    WienerFilter(size_t filterOrder = 10, double mu = 0.01, double lambda = 0.99);

    /**
     * Применить фильтр Винера к сигналу
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
     * @param filterOrder Порядок фильтра
     * @param mu Шаг адаптации
     * @param lambda Коэффициент забывания
     */
    void setParameters(size_t filterOrder, double mu, double lambda);

    /**
     * Сбросить состояние фильтра
     */
    void reset();

private:
    /**
     * Адаптивная фильтрация методом RLS (Recursive Least Squares)
     * @param input Входной сигнал
     * @return Отфильтрованный сигнал
     */
    Signal processRLS(const Signal& input);

    /**
     * Адаптивная фильтрация методом LMS (Least Mean Squares)
     * @param input Входной сигнал
     * @return Отфильтрованный сигнал
     */
    Signal processLMS(const Signal& input);

    /**
     * Создать вектор задержанных отсчетов
     * @param input Входной сигнал
     * @param index Текущий индекс
     * @return Вектор задержанных отсчетов
     */
    std::vector<double> getDelayVector(const Signal& input, size_t index) const;
};

#endif // WIENER_FILTER_H