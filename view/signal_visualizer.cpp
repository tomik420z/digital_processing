#include "signal_visualizer.h"
#include <iostream>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Вершинный шейдер
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)";

// Фрагментный шейдер
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 color;

void main() {
    FragColor = vec4(color, 1.0);
}
)";

SignalVisualizer::SignalVisualizer(int width, int height, const std::string& title)
    : window_(nullptr)
    , windowWidth_(width)
    , windowHeight_(height)
    , windowTitle_(title)
    , minY_(-2.0f)
    , maxY_(2.0f)
    , autoScale_(true)
    , zoomFactor_(1.0f)
    , offsetX_(0.0f)
    , offsetY_(0.0f)
    , minZoom_(0.1f)
    , maxZoom_(10.0f)
    , showOriginal_(true)
    , showNoisy_(true)
    , showFiltered_(true)
    , shaderProgram_(0)
    , originalVAO_(0), originalVBO_(0)
    , noisyVAO_(0), noisyVBO_(0)
    , filteredVAO_(0), filteredVBO_(0)
    , originalColor_(0.0f, 0.8f, 0.0f)  // Зеленый
    , noisyColor_(0.8f, 0.0f, 0.0f)     // Красный
    , filteredColor_(0.0f, 0.0f, 0.8f)  // Синий
{
}

SignalVisualizer::~SignalVisualizer() {
    cleanup();
}

bool SignalVisualizer::initialize() {
    if (!initializeGLFW()) {
        return false;
    }

    if (!initializeGLEW()) {
        cleanup();
        return false;
    }

    if (!createShaderProgram()) {
        cleanup();
        return false;
    }

    // Настройка OpenGL
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.5f);

    // Установка callback'ов
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);

    std::cout << "OpenGL инициализирован успешно" << std::endl;
    return true;
}

bool SignalVisualizer::initializeGLFW() {
    if (!glfwInit()) {
        std::cerr << "Ошибка инициализации GLFW" << std::endl;
        return false;
    }

    // Настройка контекста OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Создание окна
    window_ = glfwCreateWindow(windowWidth_, windowHeight_, windowTitle_.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "Ошибка создания окна GLFW" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Включение V-Sync

    return true;
}

bool SignalVisualizer::initializeGLEW() {
    if (glewInit() != GLEW_OK) {
        std::cerr << "Ошибка инициализации GLEW" << std::endl;
        return false;
    }

    std::cout << "OpenGL версия: " << glGetString(GL_VERSION) << std::endl;
    return true;
}

bool SignalVisualizer::createShaderProgram() {
    // Компилируем вершинный шейдер
    GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    if (vertexShader == 0) return false;

    // Компилируем фрагментный шейдер
    GLuint fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    // Создаем шейдерную программу
    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vertexShader);
    glAttachShader(shaderProgram_, fragmentShader);
    glLinkProgram(shaderProgram_);

    // Проверяем линковку
    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram_, 512, nullptr, infoLog);
        std::cerr << "Ошибка линковки шейдерной программы: " << infoLog << std::endl;

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    // Очищаем шейдеры (они уже скомпилированы в программу)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

GLuint SignalVisualizer::compileShader(const std::string& source, GLenum type) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // Проверяем компиляцию
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Ошибка компиляции шейдера: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

void SignalVisualizer::setSignalData(const SignalProcessor::Signal& noisy,
                                     const SignalProcessor::Signal& filtered,
                                     const SignalProcessor::Signal& original) {
    noisySignal_ = noisy;
    filteredSignal_ = filtered;
    originalSignal_ = original;

    if (autoScale_) {
        calculateAutoScale();
    }

    updateSignalBuffers();

    // Инициализируем кнопки после загрузки всех сигналов
    initializeToggleButtons();
}

void SignalVisualizer::calculateAutoScale() {
    if (noisySignal_.empty() && filteredSignal_.empty()) {
        minY_ = -2.0f;
        maxY_ = 2.0f;
        return;
    }

    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();

    // Поиск min/max среди всех сигналов
    for (const auto& signal : {noisySignal_, filteredSignal_, originalSignal_}) {
        if (!signal.empty()) {
            auto [min_it, max_it] = std::minmax_element(signal.begin(), signal.end());
            minVal = std::min(minVal, *min_it);
            maxVal = std::max(maxVal, *max_it);
        }
    }

    // Добавляем отступы 10%
    double range = maxVal - minVal;
    double padding = range * 0.1;
    minY_ = static_cast<float>(minVal - padding);
    maxY_ = static_cast<float>(maxVal + padding);

    // Предотвращаем деление на ноль
    if (std::abs(maxY_ - minY_) < 1e-6f) {
        minY_ -= 1.0f;
        maxY_ += 1.0f;
    }
}

void SignalVisualizer::createSignalBuffers(const SignalProcessor::Signal& signal, GLuint& vao, GLuint& vbo) {
    if (signal.empty()) return;

    std::vector<float> vertices;
    vertices.reserve(signal.size() * 2);

    for (size_t i = 0; i < signal.size(); ++i) {
        float x = indexToX(i, signal.size());
        float y = valueToY(signal[i]);
        vertices.push_back(x);
        vertices.push_back(y);
    }

    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void SignalVisualizer::updateSignalBuffers() {
    createSignalBuffers(originalSignal_, originalVAO_, originalVBO_);
    createSignalBuffers(noisySignal_, noisyVAO_, noisyVBO_);
    createSignalBuffers(filteredSignal_, filteredVAO_, filteredVBO_);
}

float SignalVisualizer::indexToX(size_t index, size_t signalLength) const {
    if (signalLength <= 1) return offsetX_;
    float normalizedX = -1.0f + (2.0f * index) / (signalLength - 1);
    return (normalizedX * zoomFactor_) + offsetX_;
}

float SignalVisualizer::valueToY(double value) const {
    float normalizedY = -1.0f + (2.0f * (value - minY_)) / (maxY_ - minY_);
    return (normalizedY * zoomFactor_) + offsetY_;
}

void SignalVisualizer::run() {
    std::cout << "Запуск визуализации..." << std::endl;
    std::cout << "Управление:" << std::endl;
    std::cout << "  ESC - выход" << std::endl;
    std::cout << "  R - перезагрузить данные" << std::endl;
    std::cout << "  + / = - увеличить масштаб (приблизить)" << std::endl;
    std::cout << "  - / _ - уменьшить масштаб (отдалить)" << std::endl;
    std::cout << "  Колесо мыши - зум" << std::endl;
    std::cout << "  Стрелки ↑↓←→ - панорамирование" << std::endl;
    std::cout << "  SPACE - сброс вида" << std::endl;
    std::cout << "  G - переключить чистый сигнал (зеленый)" << std::endl;
    std::cout << "  N - переключить зашумленный сигнал (красный)" << std::endl;
    std::cout << "  F - переключить отфильтрованный сигнал (синий)" << std::endl;

    while (!shouldClose()) {
        processEvents();
        render();
        glfwSwapBuffers(window_);
    }
}

bool SignalVisualizer::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void SignalVisualizer::render() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram_);

    // Рисуем сетку
    drawGrid();

    // Рисуем оси
    drawAxes();

    // Рисуем сигналы с учетом флагов видимости
    if (!originalSignal_.empty() && showOriginal_) {
        drawSignal(originalVAO_, originalSignal_.size(), originalColor_);
    }

    if (!noisySignal_.empty() && showNoisy_) {
        drawSignal(noisyVAO_, noisySignal_.size(), noisyColor_);
    }

    if (!filteredSignal_.empty() && showFiltered_) {
        drawSignal(filteredVAO_, filteredSignal_.size(), filteredColor_);
    }

    // Рисуем кнопки переключения
    drawToggleButtons();
}

void SignalVisualizer::drawSignal(GLuint vao, size_t pointCount, const Color& color) {
    if (vao == 0 || pointCount == 0) return;

    // Устанавливаем цвет
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"), color.r, color.g, color.b);

    glBindVertexArray(vao);
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(pointCount));
    glBindVertexArray(0);
}

void SignalVisualizer::drawGrid() {
    const Color gridColor(0.3f, 0.3f, 0.3f);
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"), gridColor.r, gridColor.g, gridColor.b);

    // Вертикальные линии
    for (int i = -8; i <= 8; i += 2) {
        float x = i / 10.0f;
        float vertices[] = {x, -1.0f, x, 1.0f};

        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glDrawArrays(GL_LINES, 0, 2);

        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
    }

    // Горизонтальные линии
    for (int i = -8; i <= 8; i += 2) {
        float y = i / 10.0f;
        float vertices[] = {-1.0f, y, 1.0f, y};

        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glDrawArrays(GL_LINES, 0, 2);

        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
    }
}

void SignalVisualizer::drawAxes() {
    const Color axisColor(0.7f, 0.7f, 0.7f);
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"), axisColor.r, axisColor.g, axisColor.b);
    glLineWidth(2.0f);

    // X ось
    float xAxisVertices[] = {-1.0f, 0.0f, 1.0f, 0.0f};
    GLuint xVao, xVbo;
    glGenVertexArrays(1, &xVao);
    glGenBuffers(1, &xVbo);

    glBindVertexArray(xVao);
    glBindBuffer(GL_ARRAY_BUFFER, xVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(xAxisVertices), xAxisVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, 2);

    // Y ось
    float yAxisVertices[] = {0.0f, -1.0f, 0.0f, 1.0f};
    GLuint yVao, yVbo;
    glGenVertexArrays(1, &yVao);
    glGenBuffers(1, &yVbo);

    glBindVertexArray(yVao);
    glBindBuffer(GL_ARRAY_BUFFER, yVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(yAxisVertices), yAxisVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, 2);

    glDeleteVertexArrays(1, &xVao);
    glDeleteBuffers(1, &xVbo);
    glDeleteVertexArrays(1, &yVao);
    glDeleteBuffers(1, &yVbo);

    glLineWidth(1.5f);
}

void SignalVisualizer::processEvents() {
    glfwPollEvents();
}

void SignalVisualizer::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    SignalVisualizer* visualizer = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (visualizer) {
        visualizer->windowWidth_ = width;
        visualizer->windowHeight_ = height;
    }
}

void SignalVisualizer::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    SignalVisualizer* visualizer = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!visualizer) return;

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            case GLFW_KEY_R:
                std::cout << "Обновление данных..." << std::endl;
                visualizer->updateSignalBuffers();
                break;
            case GLFW_KEY_SPACE:
                visualizer->resetView();
                break;
            case GLFW_KEY_UP:
                visualizer->pan(0.0f, 0.1f);
                break;
            case GLFW_KEY_DOWN:
                visualizer->pan(0.0f, -0.1f);
                break;
            case GLFW_KEY_LEFT:
                visualizer->pan(0.1f, 0.0f);
                break;
            case GLFW_KEY_RIGHT:
                visualizer->pan(-0.1f, 0.0f);
                break;
            case GLFW_KEY_EQUAL:  // Клавиша "+" (без shift)
            case GLFW_KEY_KP_ADD: // Клавиша "+" на цифровой клавиатуре
                visualizer->zoom(1.2f);
                break;
            case GLFW_KEY_MINUS:  // Клавиша "-"
            case GLFW_KEY_KP_SUBTRACT: // Клавиша "-" на цифровой клавиатуре
                visualizer->zoom(0.8f);
                break;
            case GLFW_KEY_G: // Переключить чистый сигнал (зеленый)
                visualizer->showOriginal_ = !visualizer->showOriginal_;
                std::cout << "Чистый сигнал: " << (visualizer->showOriginal_ ? "показан" : "скрыт") << std::endl;
                break;
            case GLFW_KEY_N: // Переключить зашумленный сигнал (красный)
                visualizer->showNoisy_ = !visualizer->showNoisy_;
                std::cout << "Зашумленный сигнал: " << (visualizer->showNoisy_ ? "показан" : "скрыт") << std::endl;
                break;
            case GLFW_KEY_F: // Переключить отфильтрованный сигнал (синий)
                visualizer->showFiltered_ = !visualizer->showFiltered_;
                std::cout << "Отфильтрованный сигнал: " << (visualizer->showFiltered_ ? "показан" : "скрыт") << std::endl;
                break;
        }
    }
}

void SignalVisualizer::cleanup() {
    if (originalVAO_ != 0) glDeleteVertexArrays(1, &originalVAO_);
    if (originalVBO_ != 0) glDeleteBuffers(1, &originalVBO_);
    if (noisyVAO_ != 0) glDeleteVertexArrays(1, &noisyVAO_);
    if (noisyVBO_ != 0) glDeleteBuffers(1, &noisyVBO_);
    if (filteredVAO_ != 0) glDeleteVertexArrays(1, &filteredVAO_);
    if (filteredVBO_ != 0) glDeleteBuffers(1, &filteredVBO_);

    if (shaderProgram_ != 0) glDeleteProgram(shaderProgram_);

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

void SignalVisualizer::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    SignalVisualizer* visualizer = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!visualizer) return;

    float zoomDelta = static_cast<float>(yoffset) * 0.1f;
    visualizer->zoom(1.0f + zoomDelta);
}

void SignalVisualizer::zoom(float factor) {
    zoomFactor_ *= factor;
    zoomFactor_ = std::max(minZoom_, std::min(maxZoom_, zoomFactor_));
    updateSignalBuffers();
}

void SignalVisualizer::pan(float deltaX, float deltaY) {
    offsetX_ += deltaX / zoomFactor_;
    offsetY_ += deltaY / zoomFactor_;
    updateSignalBuffers();
}

void SignalVisualizer::resetView() {
    zoomFactor_ = 1.0f;
    offsetX_ = 0.0f;
    offsetY_ = 0.0f;
    updateSignalBuffers();
    std::cout << "Вид сброшен" << std::endl;
}

void SignalVisualizer::updateViewTransform() {
    // Этот метод может быть использован для более сложных трансформаций
    updateSignalBuffers();
}

void SignalVisualizer::initializeToggleButtons() {
    toggleButtons_.clear();

    // Получаем размер окна для фиксированного размера кнопок в пикселях
    int windowWidth, windowHeight;
    glfwGetWindowSize(window_, &windowWidth, &windowHeight);

    // Фиксированный размер кнопки в пикселях (20 пикселей радиус)
    float pixelRadius = 20.0f;
    float buttonRadius = pixelRadius / std::min(windowWidth, windowHeight) * 2.0f;

    float startX = -0.95f;        // Левый верхний угол
    float startY = 0.9f;
    float spacing = 0.1f;         // Горизонтальное расстояние между кнопками

    // Всегда создаем все три кнопки горизонтально
    // Кнопка для чистого сигнала (зеленая)
    toggleButtons_.emplace_back(startX, startY, buttonRadius, originalColor_, &showOriginal_);

    // Кнопка для зашумленного сигнала (красная)
    toggleButtons_.emplace_back(startX + spacing, startY, buttonRadius, noisyColor_, &showNoisy_);

    // Кнопка для отфильтрованного сигнала (синяя)
    toggleButtons_.emplace_back(startX + 2 * spacing, startY, buttonRadius, filteredColor_, &showFiltered_);
}

void SignalVisualizer::drawToggleButtons() {
    for (const auto& button : toggleButtons_) {
        drawCircleButton(button);
    }
}

void SignalVisualizer::drawCircleButton(const Button& button) {
    Color renderColor = button.color;

    // Если кнопка отключена, делаем цвет блеклым
    if (!(*button.visibility)) {
        renderColor.r *= 0.3f;
        renderColor.g *= 0.3f;
        renderColor.b *= 0.3f;
    }

    glUniform3f(glGetUniformLocation(shaderProgram_, "color"), renderColor.r, renderColor.g, renderColor.b);

    // Создаем вершины для круга
    const int segments = 32;
    std::vector<float> vertices;
    vertices.reserve((segments + 2) * 2); // центр + вершины + замыкание

    // Центр круга
    vertices.push_back(button.centerX);
    vertices.push_back(button.centerY);

    // Вершины круга
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        float x = button.centerX + button.radius * std::cos(angle);
        float y = button.centerY + button.radius * std::sin(angle);
        vertices.push_back(x);
        vertices.push_back(y);
    }

    // Создаем временные VAO/VBO для круга
    GLuint circleVAO, circleVBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);

    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(vertices.size() / 2));

    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);

    // Рисуем контур кнопки
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"), 1.0f, 1.0f, 1.0f);
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);

    std::vector<float> borderVertices;
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        float x = button.centerX + button.radius * std::cos(angle);
        float y = button.centerY + button.radius * std::sin(angle);
        borderVertices.push_back(x);
        borderVertices.push_back(y);
    }

    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, borderVertices.size() * sizeof(float), borderVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(borderVertices.size() / 2));

    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);
}

void SignalVisualizer::mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
    SignalVisualizer* visualizer = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!visualizer) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        visualizer->handleMouseClick(xpos, ypos);
    }
}

bool SignalVisualizer::isPointInButton(double x, double y, const Button& button) const {
    // Преобразуем экранные координаты в нормализованные координаты
    double normalizedX = (2.0 * x) / windowWidth_ - 1.0;
    double normalizedY = 1.0 - (2.0 * y) / windowHeight_;

    float dx = static_cast<float>(normalizedX) - button.centerX;
    float dy = static_cast<float>(normalizedY) - button.centerY;
    float distance = std::sqrt(dx * dx + dy * dy);

    return distance <= button.radius;
}

void SignalVisualizer::handleMouseClick(double x, double y) {
    for (const auto& button : toggleButtons_) {
        if (isPointInButton(x, y, button)) {
            *(button.visibility) = !*(button.visibility);

            // Выводим информацию о переключении
            std::string signalName;
            if (button.visibility == &showOriginal_) {
                signalName = "Чистый сигнал";
            } else if (button.visibility == &showNoisy_) {
                signalName = "Зашумленный сигнал";
            } else if (button.visibility == &showFiltered_) {
                signalName = "Отфильтрованный сигнал";
            }

            std::cout << signalName << ": " << (*(button.visibility) ? "показан" : "скрыт") << std::endl;
            break;
        }
    }
}