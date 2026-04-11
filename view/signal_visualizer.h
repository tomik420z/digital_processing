#ifndef SIGNAL_VISUALIZER_H
#define SIGNAL_VISUALIZER_H

#include "../src/signal_processor.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

/**
 * Класс для визуализации сигналов с использованием OpenGL.
 *
 * Поддерживает два режима:
 *
 * 1. Обычный режим (splitView_ = false):
 *    Одна панель — временной сигнал (original / noisy / filtered).
 *
 * 2. Split-view (splitView_ = true, вызов enableSplitView()):
 *    Верхняя панель (60%): временной сигнал (огибающая |x[n]|)
 *      - original (чистый),  noisy (с НИП),  filtered (после компенсации)
 *    Нижняя панель (40%): доплеровский спектр (дБ)
 *      - specBefore_  (до компенсации)
 *      - specAfter_   (после компенсации)
 *      - specDiff_    (разность = подавленная НИП)
 */
class SignalVisualizer {
private:
    GLFWwindow* window_;
    int windowWidth_;    ///< Физический размер framebuffer (пиксели GPU)
    int windowHeight_;
    int logicalWidth_;   ///< Логический размер окна (для hit-test курсора)
    int logicalHeight_;
    std::string windowTitle_;

    // ── Данные временного сигнала ─────────────────────────────────────────
    SignalProcessor::Signal originalSignal_;
    SignalProcessor::Signal noisySignal_;
    SignalProcessor::Signal filteredSignal_;

    // ── Данные доплеровского спектра (дБ) ─────────────────────────────────
    SignalProcessor::Signal specBefore_;   ///< Спектр до компенсации
    SignalProcessor::Signal specAfter_;    ///< Спектр после компенсации
    SignalProcessor::Signal specDiff_;     ///< Разность (подавленная НИП)

    // ── Параметры отображения (временная панель) ──────────────────────────
    float minY_, maxY_;
    bool autoScale_;

    // ── Параметры отображения (спектральная панель) ───────────────────────
    float specMinY_, specMaxY_;

    // ── Параметры зума ────────────────────────────────────────────────────
    float zoomFactor_;
    float offsetX_;
    float offsetY_;
    float minZoom_;
    float maxZoom_;

    // ── Флаги видимости сигналов (временная панель) ───────────────────────
    bool showOriginal_;
    bool showNoisy_;
    bool showFiltered_;

    // ── Флаги видимости спектральных кривых ──────────────────────────────
    bool showSpecBefore_;
    bool showSpecAfter_;
    bool showSpecDiff_;

    // ── Режим split-view ──────────────────────────────────────────────────
    bool splitView_;           ///< true → верхняя (60%) + нижняя (40%) панели
    float splitRatio_;         ///< Доля высоты для верхней панели (0..1, по умолч. 0.6)

    // ── OpenGL объекты (временные кривые) ─────────────────────────────────
    GLuint shaderProgram_;
    GLuint originalVAO_, originalVBO_;
    GLuint noisyVAO_,    noisyVBO_;
    GLuint filteredVAO_, filteredVBO_;

    // ── OpenGL объекты (спектральные кривые) ──────────────────────────────
    GLuint specBeforeVAO_, specBeforeVBO_;
    GLuint specAfterVAO_,  specAfterVBO_;
    GLuint specDiffVAO_,   specDiffVBO_;

    // ── Шейдерная программа для текстурных кнопок ────────────────────────
    GLuint textureShaderProgram_;

    // ── Цвета ─────────────────────────────────────────────────────────────
    struct Color {
        float r, g, b;
        Color(float r, float g, float b) : r(r), g(g), b(b) {}
    };

    // ── Кнопки переключения ────────────────────────────────────────────────
    struct Button {
        float centerX, centerY;
        float halfW, halfH;
        Color color;
        bool* visibility;
        GLuint textureID;
        bool   isHovered;

        Button(float x, float y, float hw, float hh, const Color& c, bool* vis)
            : centerX(x), centerY(y), halfW(hw), halfH(hh),
              color(c), visibility(vis), textureID(0), isHovered(false) {}
    };

    std::vector<Button> toggleButtons_;   ///< Кнопки временной панели
    std::vector<Button> specButtons_;     ///< Кнопки спектральной панели

    Color originalColor_;    // зелёный
    Color noisyColor_;       // красный
    Color filteredColor_;    // синий
    Color specBeforeColor_;  // красный (спектр до)
    Color specAfterColor_;   // синий (спектр после)
    Color specDiffColor_;    // жёлтый (разность)

public:
    /**
     * Конструктор
     * @param width  Ширина окна
     * @param height Высота окна
     * @param title  Заголовок окна
     */
    SignalVisualizer(int width  = 1200,
                     int height = 800,
                     const std::string& title = "Signal Filter Visualization");

    ~SignalVisualizer();

    // ── Инициализация ─────────────────────────────────────────────────────

    bool initialize();

    // ── Данные временного сигнала ─────────────────────────────────────────

    /**
     * Установить данные временного сигнала (обычный режим).
     */
    void setSignalData(const SignalProcessor::Signal& noisy,
                       const SignalProcessor::Signal& filtered,
                       const SignalProcessor::Signal& original = {});

    // ── Split-view API ────────────────────────────────────────────────────

    /**
     * Включить режим split-view.
     * Вызывать ПОСЛЕ setSignalData().
     *
     * @param specBefore  Доплеровский спектр до компенсации (дБ), размер N
     * @param specAfter   Доплеровский спектр после компенсации (дБ), размер N
     * @param specDiff    Разность спектров до−после (дБ), размер N
     * @param ratio       Доля высоты для верхней панели (0.4..0.8, по умолч. 0.6)
     */
    void enableSplitView(const SignalProcessor::Signal& specBefore,
                         const SignalProcessor::Signal& specAfter,
                         const SignalProcessor::Signal& specDiff,
                         float ratio = 0.6f);

    /**
     * Отключить split-view (вернуться к обычному режиму).
     */
    void disableSplitView();

    // ── Цикл работы ──────────────────────────────────────────────────────

    void run();
    bool shouldClose() const;
    void render();
    void processEvents();

private:
    // ── Инициализация GL ──────────────────────────────────────────────────
    bool initializeGLFW();
    bool initializeGLEW();
    bool createShaderProgram();
    bool createTextureShaderProgram();
    GLuint compileShader(const std::string& source, GLenum type);
    GLuint loadTexture(const std::string& path);

    // ── Буферы ────────────────────────────────────────────────────────────
    void createSignalBuffers(const SignalProcessor::Signal& signal,
                             GLuint& vao, GLuint& vbo,
                             float yMin, float yMax,
                             float xMin = -1.0f, float xMax = 1.0f);
    void updateSignalBuffers();
    void updateSpectrumBuffers();

    // ── Масштабирование ───────────────────────────────────────────────────
    void calculateAutoScale();
    void calculateSpectrumScale();

    // ── Координатные преобразования ───────────────────────────────────────
    /**
     * Преобразование индекса → X в диапазоне [-1, +1].
     */
    float indexToX(size_t index, size_t signalLength) const;

    /**
     * Преобразование значения → Y в панельных координатах [-1, +1]
     * относительно заданного диапазона [yMin, yMax].
     */
    float valueToY(double value, float yMin, float yMax) const;

    // ── Отрисовка ─────────────────────────────────────────────────────────
    void drawTopPanel();
    void drawBottomPanel();

    void drawSignal(GLuint vao, size_t pointCount, const Color& color);
    void drawGrid(float vpX, float vpY, float vpW, float vpH);
    void drawAxes(float vpX, float vpY, float vpW, float vpH);

    void drawToggleButtons();
    void drawSpecButtons();
    void drawRectButton(const Button& button);

    // ── Кнопки ────────────────────────────────────────────────────────────
    void initializeToggleButtons();
    void initializeSpecButtons();
    void cleanupButtonTextures();

    bool isPointInButton(double x, double y, const Button& button) const;
    bool isPointInSpecButton(double x, double y, const Button& btn) const;
    void handleMouseClick(double x, double y);
    void updateHoverState(double x, double y);

    // ── Зум / пан ─────────────────────────────────────────────────────────
    void zoom(float factor);
    void pan(float deltaX, float deltaY);
    void resetView();
    void updateViewTransform();

    // ── Утилиты ───────────────────────────────────────────────────────────
    void cleanup();

    // ── Колбэки GLFW ──────────────────────────────────────────────────────
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode,
                            int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void mouseButtonCallback(GLFWwindow* window, int button,
                                    int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
};

#endif // SIGNAL_VISUALIZER_H
