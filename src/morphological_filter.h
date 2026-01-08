#ifndef MORPHOLOGICAL_FILTER_H
#define MORPHOLOGICAL_FILTER_H

#include "signal_processor.h"

/**
 * Морфологические фильтры для подавления импульсных помех
 */
class MorphologicalFilter : public SignalProcessor {
public:
    /**
     * Типы морфологических операций
     */
    enum class Operation {
        OPENING,    // Размыкание (эрозия + дилатация)
        CLOSING,    // Замыкание (дилатация + эрозия)
        EROSION,    // Эрозия
        DILATION    // Дилатация
    };

private:
    Operation operation_;           // Тип операции
    std::vector<double> structuringElement_; // Структурирующий элемент

public:
    /**
     * Конструктор
     * @param operation Тип морфологической операции
     * @param elementSize Размер структурирующего элемента
     */
    MorphologicalFilter(Operation operation = Operation::OPENING, size_t elementSize = 5);

    /**
     * Конструктор с пользовательским структурирующим элементом
     * @param operation Тип морфологической операции
     * @param structuringElement Структурирующий элемент
     */
    MorphologicalFilter(Operation operation, const std::vector<double>& structuringElement);

    /**
     * Применить морфологический фильтр к сигналу
     * @param input Входной сигнал
     * @return Отфильтрованный сигнал
     */
    Signal process(const Signal& input) override;

    /**
     * Получить имя алгоритма
     */
    std::string getName() const override;

    /**
     * Установить тип операции
     * @param operation Новый тип операции
     */
    void setOperation(Operation operation);

    /**
     * Установить структурирующий элемент
     * @param structuringElement Новый структурирующий элемент
     */
    void setStructuringElement(const std::vector<double>& structuringElement);

private:
    /**
     * Эрозия сигнала
     * @param input Входной сигнал
     * @return Результат эрозии
     */
    Signal erosion(const Signal& input) const;

    /**
     * Дилатация сигнала
     * @param input Входной сигнал
     * @return Результат дилатации
     */
    Signal dilation(const Signal& input) const;

    /**
     * Размыкание (эрозия + дилатация)
     * @param input Входной сигнал
     * @return Результат размыкания
     */
    Signal opening(const Signal& input) const;

    /**
     * Замыкание (дилатация + эрозия)
     * @param input Входной сигнал
     * @return Результат замыкания
     */
    Signal closing(const Signal& input) const;

    /**
     * Создать плоский структурирующий элемент
     * @param size Размер элемента
     * @return Структурирующий элемент
     */
    static std::vector<double> createFlatElement(size_t size);

    /**
     * Получить название операции
     * @param operation Тип операции
     * @return Строковое представление
     */
    static std::string operationToString(Operation operation);
};

#endif // MORPHOLOGICAL_FILTER_H