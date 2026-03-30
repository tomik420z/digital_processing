#ifndef SIGNAL_VISUALIZER_H
#define SIGNAL_VISUALIZER_H

#include "../src/signal_processor.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

/**
 * Класс для визуализации сигналов с использованием OpenGL
 */
class SignalVisualizer {
private:
    GLFWwindow* window_;
    int windowWidth_;
    int windowHeight_;
    std::string windowTitle_;

    // Данные сигналов
    SignalProcessor::Signal originalSignal_;
    SignalProcessor::Signal noisySignal_;
    SignalProcessor::Signal filteredSignal_;

    // Параметры отображения
    float minY_, maxY_;
    bool autoScale_;

    // Параметры зума
    float zoomFactor_;
    float offsetX_;
    float offsetY_;
    float minZoom_;
    float maxZoom_;

    // Флаги видимости сигналов
    bool showOriginal_;
    bool showNoisy_;
    bool showFiltered_;

    // OpenGL объекты (для рисования сигналов/сетки)
    GLuint shaderProgram_;
    GLuint originalVAO_, originalVBO_;
    GLuint noisyVAO_, noisyVBO_;
    GLuint filteredVAO_, filteredVBO_;

    // Шейдерная программа для отрисовки текстур кнопок
    GLuint textureShaderProgram_;

    // Цвета для сигналов
    struct Color {
        float r, g, b;
        Color(float r, float g, float b) : r(r), g(g), b(b) {}
    };

    // Параметры прямоугольной кнопки с PNG-иконкой
    struct Button {
        float centerX, centerY;
        float halfW, halfH;   // полуширина и полувысота в NDC
        Color color;
        bool* visibility;
        GLuint textureID;     // OpenGL ID текстуры из PNG
        bool   isHovered;     // курсор над кнопкой?

        Button(float x, float y, float hw, float hh, const Color& c, bool* vis)
            : centerX(x), centerY(y), halfW(hw), halfH(hh),
              color(c), visibility(vis), textureID(0), isHovered(false) {}
    };

    std::vector<Button> toggleButtons_;

    Color originalColor_;   // Зеленый для чистого сигнала
    Color noisyColor_;      // Красный для зашумленного сигнала
    Color filteredColor_;   // Синий для отфильтрованного сигнала

public:
    /**
     * Конструктор
     * @param width Ширина окна
     * @param height Высота окна
     * @param title Заголовок окна
     */
    SignalVisualizer(int width = 1200, int height = 800, const std::string& title = "Signal Filter Visualization");

    /**
     * Деструктор
     */
    ~SignalVisualizer();

    /**
     * Инициализация OpenGL контекста
     * @return true при успехе, false при ошибке
     */
    bool initialize();

    /**
     * Установить данные сигналов для отображения
     * @param noisy     Зашумлённый сигнал
     * @param filtered  Отфильтрованный сигнал
     * @param original  Чистый сигнал (опционально)
     */
    void setSignalData(const SignalProcessor::Signal& noisy,
                      const SignalProcessor::Signal& filtered,
                      const SignalProcessor::Signal& original = {});

    /**
     * Основной цикл отображения
     */
    void run();

    /**
     * Проверка, должно ли окно закрываться
     */
    bool shouldClose() const;

    /**
     * Отрисовка одного кадра
     */
    void render();

    /**
     * Обработка событий
     */
    void processEvents();

private:
    /**
     * Инициализация GLFW
     */
    bool initializeGLFW();

    /**
     * Инициализация GLEW
     */
    bool initializeGLEW();

    /**
     * Создание шейдерной программы для линий/сетки
     */
    bool createShaderProgram();

    /**
     * Создание шейдерной программы для текстурных кнопок
     */
    bool createTextureShaderProgram();

    /**
     * Компиляция шейдера
     */
    GLuint compileShader(const std::string& source, GLenum type);

    /**
     * Создание VAO/VBO для сигнала
     */
    void createSignalBuffers(const SignalProcessor::Signal& signal, GLuint& vao, GLuint& vbo);

    /**
     * Обновление буферов сигналов
     */
    void updateSignalBuffers();

    /**
     * Автоматическое масштабирование сигналов
     */
    void calculateAutoScale();

    /**
     * Преобразование индекса сигнала в координату X экрана
     */
    float indexToX(size_t index, size_t signalLength) const;

    /**
     * Преобразование значения сигнала в координату Y экрана
     */
    float valueToY(double value) const;

    /**
     * Отрисовка сигнала
     */
    void drawSignal(GLuint vao, size_t pointCount, const Color& color);

    /**
     * Отрисовка сетки координат
     */
    void drawGrid();

    /**
     * Отрисовка осей координат
     */
    void drawAxes();

    /**
     * Callback для изменения размера окна
     */
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    /**
     * Callback для обработки клавиш
     */
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    /**
     * Callback для обработки колеса мыши
     */
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    /**
     * Callback для обработки кликов мыши
     */
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

    /**
     * Callback для отслеживания позиции курсора (hover)
     */
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);

    /**
     * Обновить состояние hover для всех кнопок
     */
    void updateHoverState(double x, double y);

    /**
     * Инициализация кнопок переключения (загрузка PNG текстур)
     */
    void initializeToggleButtons();

    /**
     * Загрузка PNG файла как текстуры OpenGL
     * @return textureID или 0 при ошибке
     */
    GLuint loadTexture(const std::string& path);

    /**
     * Отрисовка кнопок переключения
     */
    void drawToggleButtons();

    /**
     * Отрисовка прямоугольной кнопки с PNG-иконкой
     */
    void drawRectButton(const Button& button);

    /**
     * Проверка клика по кнопке
     */
    bool isPointInButton(double x, double y, const Button& button) const;

    /**
     * Обработка клика мыши
     */
    void handleMouseClick(double x, double y);

    /**
     * Увеличить/уменьшить масштаб
     */
    void zoom(float factor);

    /**
     * Панорамирование
     */
    void pan(float deltaX, float deltaY);

    /**
     * Сбросить зум и позицию
     */
    void resetView();

    /**
     * Обновить матрицы трансформации с учетом зума и панорамирования
     */
    void updateViewTransform();

    /**
     * Освобождение текстур кнопок
     */
    void cleanupButtonTextures();

    /**
     * Очистка ресурсов OpenGL
     */
    void cleanup();
};

#endif // SIGNAL_VISUALIZER_H
