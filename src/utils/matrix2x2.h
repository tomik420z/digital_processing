#ifndef MATRIX2X2_H
#define MATRIX2X2_H

#include <vector>
#include <iostream>

/**
 * Класс для работы с матрицами 2x2
 * Предоставляет основные матричные операции для фильтра Калмана
 */
class Matrix2x2 {
public:
    /**
     * Конструктор по умолчанию - создает нулевую матрицу
     */
    Matrix2x2();

    /**
     * Конструктор с инициализацией элементов
     * @param a00 Элемент [0,0]
     * @param a01 Элемент [0,1]
     * @param a10 Элемент [1,0]
     * @param a11 Элемент [1,1]
     */
    Matrix2x2(double a00, double a01, double a10, double a11);

    /**
     * Конструктор из std::vector<std::vector<double>>
     * @param matrix Исходная матрица
     */
    explicit Matrix2x2(const std::vector<std::vector<double>>& matrix);

    /**
     * Получить элемент по индексам
     * @param row Строка (0 или 1)
     * @param col Столбец (0 или 1)
     * @return Ссылка на элемент
     */
    double& operator()(size_t row, size_t col);

    /**
     * Получить элемент по индексам (const версия)
     * @param row Строка (0 или 1)
     * @param col Столбец (0 или 1)
     * @return Значение элемента
     */
    const double& operator()(size_t row, size_t col) const;

    /**
     * Оператор сложения матриц
     * @param other Другая матрица
     * @return Результат сложения
     */
    Matrix2x2 operator+(const Matrix2x2& other) const;

    /**
     * Оператор вычитания матриц
     * @param other Другая матрица
     * @return Результат вычитания
     */
    Matrix2x2 operator-(const Matrix2x2& other) const;

    /**
     * Оператор умножения матриц
     * @param other Другая матрица
     * @return Результат умножения
     */
    Matrix2x2 operator*(const Matrix2x2& other) const;

    /**
     * Оператор умножения на скаляр
     * @param scalar Скаляр
     * @return Результат умножения
     */
    Matrix2x2 operator*(double scalar) const;

    /**
     * Оператор присваивания
     * @param other Другая матрица
     * @return Ссылка на текущую матрицу
     */
    Matrix2x2& operator=(const Matrix2x2& other);

    /**
     * Умножение матрицы на вектор 2x1
     * @param vector Вектор [x, y]
     * @return Результирующий вектор
     */
    std::vector<double> operator*(const std::vector<double>& vector) const;

    /**
     * Транспонирование матрицы
     * @return Транспонированная матрица
     */
    Matrix2x2 transpose() const;

    /**
     * Обращение матрицы 2x2
     * @return Обращенная матрица
     * @throws std::runtime_error если матрица вырождена
     */
    Matrix2x2 inverse() const;

    /**
     * Вычисление определителя
     * @return Определитель матрицы
     */
    double determinant() const;

    /**
     * Создание единичной матрицы
     * @return Единичная матрица 2x2
     */
    static Matrix2x2 identity();

    /**
     * Создание нулевой матрицы
     * @return Нулевая матрица 2x2
     */
    static Matrix2x2 zeros();

    /**
     * Преобразование в std::vector<std::vector<double>>
     * @return Матрица в виде вложенного вектора
     */
    std::vector<std::vector<double>> toVector() const;

    /**
     * Оператор вывода в поток
     * @param os Поток вывода
     * @param matrix Матрица для вывода
     * @return Ссылка на поток
     */
    friend std::ostream& operator<<(std::ostream& os, const Matrix2x2& matrix);

    /**
     * Проверка на равенство с заданной точностью
     * @param other Другая матрица
     * @param epsilon Точность сравнения
     * @return true если матрицы равны
     */
    bool isEqual(const Matrix2x2& other, double epsilon = 1e-12) const;

private:
    double data_[2][2]; // Элементы матрицы
};

/**
 * Класс для работы с векторами 2x1
 */
class Vector2D {
public:
    /**
     * Конструктор по умолчанию - создает нулевой вектор
     */
    Vector2D();

    /**
     * Конструктор с инициализацией
     * @param x Первый элемент
     * @param y Второй элемент
     */
    Vector2D(double x, double y);

    /**
     * Конструктор из std::vector<double>
     * @param vector Исходный вектор
     */
    explicit Vector2D(const std::vector<double>& vector);

    /**
     * Доступ к элементу по индексу
     * @param index Индекс (0 или 1)
     * @return Ссылка на элемент
     */
    double& operator[](size_t index);

    /**
     * Доступ к элементу по индексу (const версия)
     * @param index Индекс (0 или 1)
     * @return Значение элемента
     */
    const double& operator[](size_t index) const;

    /**
     * Сложение векторов
     * @param other Другой вектор
     * @return Результат сложения
     */
    Vector2D operator+(const Vector2D& other) const;

    /**
     * Вычитание векторов
     * @param other Другой вектор
     * @return Результат вычитания
     */
    Vector2D operator-(const Vector2D& other) const;

    /**
     * Умножение на скаляр
     * @param scalar Скаляр
     * @return Результат умножения
     */
    Vector2D operator*(double scalar) const;

    /**
     * Скалярное произведение
     * @param other Другой вектор
     * @return Скалярное произведение
     */
    double dot(const Vector2D& other) const;

    /**
     * Преобразование в std::vector<double>
     * @return Вектор в стандартном формате
     */
    std::vector<double> toVector() const;

    /**
     * Получить x компонент
     * @return x компонент
     */
    double x() const { return data_[0]; }

    /**
     * Получить y компонент
     * @return y компонент
     */
    double y() const { return data_[1]; }

    /**
     * Установить x компонент
     * @param x Новое значение x
     */
    void setX(double x) { data_[0] = x; }

    /**
     * Установить y компонент
     * @param y Новое значение y
     */
    void setY(double y) { data_[1] = y; }

    /**
     * Оператор вывода в поток
     * @param os Поток вывода
     * @param vector Вектор для вывода
     * @return Ссылка на поток
     */
    friend std::ostream& operator<<(std::ostream& os, const Vector2D& vector);

private:
    double data_[2]; // Элементы вектора
};

#endif // MATRIX2X2_H