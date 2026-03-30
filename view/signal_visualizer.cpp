#include "signal_visualizer.h"
#include "stb_image.h"
#include <iostream>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Определяем путь к папке с изображениями относительно ROOT_PATH
// ROOT_PATH задаётся через CMake (add_compile_definitions)
#ifndef ROOT_PATH
#define ROOT_PATH "."
#endif

static const std::string IMG_DIR = std::string(ROOT_PATH) + "/view/images/";

// ── Шейдер для линий/сетки ────────────────────────────────────────────────
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 color;

void main() {
    FragColor = vec4(color, 1.0);
}
)";

// ── Шейдер для текстурных кнопок ─────────────────────────────────────────
const char* texVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* texFragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D texSampler;
uniform float dimFactor;   // 1.0 = полный цвет, 0.4 = затемнённый (кнопка выкл.)
uniform float hoverGlow;   // 0.0 = нет hover, 1.0 = hover

void main() {
    vec4 texColor = texture(texSampler, TexCoord);
    vec3 rgb = texColor.rgb * dimFactor;
    // При hover добавляем белый оттенок
    rgb = mix(rgb, rgb + vec3(0.25), hoverGlow);
    FragColor = vec4(rgb, texColor.a);
}
)";

// ═════════════════════════════════════════════════════════════════════════════

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
    , textureShaderProgram_(0)
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

    if (!createTextureShaderProgram()) {
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
    glfwSetCursorPosCallback(window_, cursorPosCallback);

    std::cout << "OpenGL инициализирован успешно" << std::endl;
    return true;
}

bool SignalVisualizer::initializeGLFW() {
    if (!glfwInit()) {
        std::cerr << "Ошибка инициализации GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window_ = glfwCreateWindow(windowWidth_, windowHeight_, windowTitle_.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "Ошибка создания окна GLFW" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

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
    GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    if (vertexShader == 0) return false;

    GLuint fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vertexShader);
    glAttachShader(shaderProgram_, fragmentShader);
    glLinkProgram(shaderProgram_);

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

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

bool SignalVisualizer::createTextureShaderProgram() {
    GLuint vs = compileShader(texVertexShaderSource, GL_VERTEX_SHADER);
    if (vs == 0) return false;

    GLuint fs = compileShader(texFragmentShaderSource, GL_FRAGMENT_SHADER);
    if (fs == 0) {
        glDeleteShader(vs);
        return false;
    }

    textureShaderProgram_ = glCreateProgram();
    glAttachShader(textureShaderProgram_, vs);
    glAttachShader(textureShaderProgram_, fs);
    glLinkProgram(textureShaderProgram_);

    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(textureShaderProgram_, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(textureShaderProgram_, 512, nullptr, infoLog);
        std::cerr << "Ошибка линковки текстурного шейдера: " << infoLog << std::endl;
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Указываем unit текстуры
    glUseProgram(textureShaderProgram_);
    glUniform1i(glGetUniformLocation(textureShaderProgram_, "texSampler"), 0);

    return true;
}

GLuint SignalVisualizer::compileShader(const std::string& source, GLenum type) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

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

GLuint SignalVisualizer::loadTexture(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4); // принудительно RGBA
    if (!data) {
        std::cerr << "Не удалось загрузить текстуру: " << path
                  << " (" << stbi_failure_reason() << ")" << std::endl;
        return 0;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "Загружена текстура: " << path
              << " [" << width << "x" << height << "]" << std::endl;
    return texID;
}

void SignalVisualizer::setSignalData(const SignalProcessor::Signal& noisy,
                                     const SignalProcessor::Signal& filtered,
                                     const SignalProcessor::Signal& original) {
    noisySignal_    = noisy;
    filteredSignal_ = filtered;
    originalSignal_ = original;

    if (autoScale_) {
        calculateAutoScale();
    }

    updateSignalBuffers();
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

    for (const auto& signal : {noisySignal_, filteredSignal_, originalSignal_}) {
        if (!signal.empty()) {
            auto [min_it, max_it] = std::minmax_element(signal.begin(), signal.end());
            minVal = std::min(minVal, *min_it);
            maxVal = std::max(maxVal, *max_it);
        }
    }

    double range   = maxVal - minVal;
    double padding = range * 0.1;
    minY_ = static_cast<float>(minVal - padding);
    maxY_ = static_cast<float>(maxVal + padding);

    if (std::abs(maxY_ - minY_) < 1e-6f) {
        minY_ -= 1.0f;
        maxY_ += 1.0f;
    }
}

void SignalVisualizer::createSignalBuffers(const SignalProcessor::Signal& signal,
                                           GLuint& vao, GLuint& vbo) {
    if (signal.empty()) return;

    std::vector<float> vertices;
    vertices.reserve(signal.size() * 2);

    for (size_t i = 0; i < signal.size(); ++i) {
        vertices.push_back(indexToX(i, signal.size()));
        vertices.push_back(valueToY(signal[i]));
    }

    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void SignalVisualizer::updateSignalBuffers() {
    createSignalBuffers(originalSignal_, originalVAO_, originalVBO_);
    createSignalBuffers(noisySignal_,    noisyVAO_,    noisyVBO_);
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
    std::cout << "  + / = - увеличить масштаб" << std::endl;
    std::cout << "  - / _ - уменьшить масштаб" << std::endl;
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
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram_);

    drawGrid();
    drawAxes();

    if (!originalSignal_.empty() && showOriginal_) {
        drawSignal(originalVAO_, originalSignal_.size(), originalColor_);
    }
    if (!noisySignal_.empty() && showNoisy_) {
        drawSignal(noisyVAO_, noisySignal_.size(), noisyColor_);
    }
    if (!filteredSignal_.empty() && showFiltered_) {
        drawSignal(filteredVAO_, filteredSignal_.size(), filteredColor_);
    }

    drawToggleButtons();
}

void SignalVisualizer::drawSignal(GLuint vao, size_t pointCount, const Color& color) {
    if (vao == 0 || pointCount == 0) return;

    glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                color.r, color.g, color.b);

    glBindVertexArray(vao);
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(pointCount));
    glBindVertexArray(0);
}

void SignalVisualizer::drawGrid() {
    const Color gridColor(0.2f, 0.2f, 0.25f);
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                gridColor.r, gridColor.g, gridColor.b);

    auto drawLine = [](float x0, float y0, float x1, float y1) {
        float verts[] = {x0, y0, x1, y1};
        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, 2);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
    };

    for (int i = -8; i <= 8; i += 2) {
        float x = i / 10.0f;
        drawLine(x, -1.0f, x, 1.0f);
        float y = i / 10.0f;
        drawLine(-1.0f, y, 1.0f, y);
    }
}

void SignalVisualizer::drawAxes() {
    const Color axisColor(0.5f, 0.5f, 0.55f);
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                axisColor.r, axisColor.g, axisColor.b);
    glLineWidth(2.0f);

    auto drawLine = [](float x0, float y0, float x1, float y1) {
        float verts[] = {x0, y0, x1, y1};
        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, 2);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
    };

    drawLine(-1.0f, 0.0f, 1.0f, 0.0f);
    drawLine(0.0f, -1.0f, 0.0f, 1.0f);

    glLineWidth(1.5f);
}

void SignalVisualizer::processEvents() {
    glfwPollEvents();
}

void SignalVisualizer::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    SignalVisualizer* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (vis) {
        vis->windowWidth_  = width;
        vis->windowHeight_ = height;
    }
}

void SignalVisualizer::keyCallback(GLFWwindow* window, int key, int /*scancode*/,
                                   int action, int /*mods*/) {
    SignalVisualizer* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!vis) return;

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            case GLFW_KEY_R:
                vis->updateSignalBuffers();
                break;
            case GLFW_KEY_SPACE:
                vis->resetView();
                break;
            case GLFW_KEY_UP:
                vis->pan(0.0f, -0.1f);
                break;
            case GLFW_KEY_DOWN:
                vis->pan(0.0f, 0.1f);
                break;
            case GLFW_KEY_LEFT:
                vis->pan( 0.1f, 0.0f);
                break;
            case GLFW_KEY_RIGHT:
                vis->pan(-0.1f, 0.0f);
                break;
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD:
                vis->zoom(1.2f);
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT:
                vis->zoom(0.8f);
                break;
            case GLFW_KEY_G:
                vis->showOriginal_ = !vis->showOriginal_;
                std::cout << "Чистый сигнал: "
                          << (vis->showOriginal_ ? "показан" : "скрыт") << std::endl;
                break;
            case GLFW_KEY_N:
                vis->showNoisy_ = !vis->showNoisy_;
                std::cout << "Зашумленный сигнал: "
                          << (vis->showNoisy_ ? "показан" : "скрыт") << std::endl;
                break;
            case GLFW_KEY_F:
                vis->showFiltered_ = !vis->showFiltered_;
                std::cout << "Отфильтрованный сигнал: "
                          << (vis->showFiltered_ ? "показан" : "скрыт") << std::endl;
                break;
        }
    }
}

void SignalVisualizer::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    SignalVisualizer* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!vis) return;
    vis->zoom(1.0f + static_cast<float>(yoffset) * 0.1f);
}

void SignalVisualizer::zoom(float factor) {
    zoomFactor_ = std::max(minZoom_, std::min(maxZoom_, zoomFactor_ * factor));
    updateSignalBuffers();
}

void SignalVisualizer::pan(float deltaX, float deltaY) {
    offsetX_ += deltaX / zoomFactor_;
    offsetY_ += deltaY / zoomFactor_;
    updateSignalBuffers();
}

void SignalVisualizer::resetView() {
    zoomFactor_ = 1.0f;
    offsetX_    = 0.0f;
    offsetY_    = 0.0f;
    updateSignalBuffers();
    std::cout << "Вид сброшен" << std::endl;
}

void SignalVisualizer::updateViewTransform() {
    updateSignalBuffers();
}

// ── Инициализация кнопок — загружаем PNG ──────────────────────────────────
void SignalVisualizer::initializeToggleButtons() {
    cleanupButtonTextures();
    toggleButtons_.clear();

    // Размеры кнопки в NDC: 80x40 пикселей → соотношение 2:1
    float halfW  = 0.12f;
    float halfH  = 0.065f;
    float startX = -0.82f;
    float startY =  0.88f;
    float spacing = 0.30f;

    struct BtnInfo {
        float cx;
        std::string imgFile;
        Color color;
        bool* flag;
    };

    BtnInfo infos[] = {
        { startX,             "btn_clean.png",    originalColor_, &showOriginal_ },
        { startX + spacing,   "btn_noisy.png",    noisyColor_,    &showNoisy_    },
        { startX + 2*spacing, "btn_filtered.png", filteredColor_, &showFiltered_ },
    };

    for (auto& info : infos) {
        Button btn(info.cx, startY, halfW, halfH, info.color, info.flag);
        btn.textureID = loadTexture(IMG_DIR + info.imgFile);
        toggleButtons_.push_back(std::move(btn));
    }
}

// ── Отрисовка всех кнопок ─────────────────────────────────────────────────
void SignalVisualizer::drawToggleButtons() {
    for (const auto& btn : toggleButtons_) {
        drawRectButton(btn);
    }
}

// ── Отрисовка прямоугольной кнопки с PNG-текстурой ───────────────────────
void SignalVisualizer::drawRectButton(const Button& button) {
    float x0 = button.centerX - button.halfW;
    float x1 = button.centerX + button.halfW;
    float y0 = button.centerY - button.halfH;
    float y1 = button.centerY + button.halfH;

    bool active = *(button.visibility);

    if (button.textureID != 0) {
        // ── Режим текстуры ────────────────────────────────────────────────
        // Вершины: (x, y, u, v)
        float verts[] = {
            x0, y0,  0.0f, 0.0f,
            x1, y0,  1.0f, 0.0f,
            x0, y1,  0.0f, 1.0f,
            x1, y1,  1.0f, 1.0f,
        };

        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        // aPos — location 0, aTexCoord — location 1
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glUseProgram(textureShaderProgram_);

        // Яркость: 1.0 если кнопка «включена», 0.4 если «выключена»
        float dim = active ? 1.0f : 0.4f;
        glUniform1f(glGetUniformLocation(textureShaderProgram_, "dimFactor"), dim);
        float hov = button.isHovered ? 1.0f : 0.0f;
        glUniform1f(glGetUniformLocation(textureShaderProgram_, "hoverGlow"), hov);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, button.textureID);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);

        // Вернуть основной шейдер
        glUseProgram(shaderProgram_);
    } else {
        // ── Fallback: цветной прямоугольник ──────────────────────────────
        Color fillColor = button.color;
        if (!active) {
            fillColor.r *= 0.3f;
            fillColor.g *= 0.3f;
            fillColor.b *= 0.3f;
        }
        glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                    fillColor.r, fillColor.g, fillColor.b);

        float fillVerts[] = { x0, y0, x1, y0, x0, y1, x1, y1 };
        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fillVerts), fillVerts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
    }

    // ── Контур вокруг кнопки: ярко-белый при hover, иначе обычный ────────
    glUseProgram(shaderProgram_);
    if (button.isHovered) {
        glUniform3f(glGetUniformLocation(shaderProgram_, "color"), 1.0f, 1.0f, 1.0f);
        glLineWidth(3.0f);
    } else {
        float borderAlpha = active ? 1.0f : 0.4f;
        glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                    borderAlpha, borderAlpha, borderAlpha);
        glLineWidth(active ? 2.0f : 1.0f);
    }

    float borderVerts[] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    GLuint bvao, bvbo;
    glGenVertexArrays(1, &bvao);
    glGenBuffers(1, &bvbo);
    glBindVertexArray(bvao);
    glBindBuffer(GL_ARRAY_BUFFER, bvbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(borderVerts), borderVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
    glDeleteVertexArrays(1, &bvao);
    glDeleteBuffers(1, &bvbo);

    glLineWidth(1.5f);
}

// ── Cursor hover callbacks ────────────────────────────────────────────────
void SignalVisualizer::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    SignalVisualizer* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!vis) return;
    vis->updateHoverState(xpos, ypos);
}

void SignalVisualizer::updateHoverState(double x, double y) {
    for (auto& btn : toggleButtons_) {
        btn.isHovered = isPointInButton(x, y, btn);
    }
}

void SignalVisualizer::mouseButtonCallback(GLFWwindow* window, int button,
                                           int action, int /*mods*/) {
    SignalVisualizer* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!vis) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        vis->handleMouseClick(xpos, ypos);
    }
}

bool SignalVisualizer::isPointInButton(double x, double y, const Button& button) const {
    float nx = static_cast<float>((2.0 * x) / windowWidth_  - 1.0);
    float ny = static_cast<float>(1.0 - (2.0 * y) / windowHeight_);

    return (nx >= button.centerX - button.halfW &&
            nx <= button.centerX + button.halfW &&
            ny >= button.centerY - button.halfH &&
            ny <= button.centerY + button.halfH);
}

void SignalVisualizer::handleMouseClick(double x, double y) {
    for (const auto& button : toggleButtons_) {
        if (isPointInButton(x, y, button)) {
            *(button.visibility) = !*(button.visibility);

            std::string name;
            if (button.visibility == &showOriginal_)  name = "Чистый сигнал";
            else if (button.visibility == &showNoisy_) name = "Зашумленный сигнал";
            else if (button.visibility == &showFiltered_) name = "Отфильтрованный сигнал";

            std::cout << name << ": "
                      << (*(button.visibility) ? "показан" : "скрыт") << std::endl;
            break;
        }
    }
}

// ── Освобождение текстур кнопок ───────────────────────────────────────────
void SignalVisualizer::cleanupButtonTextures() {
    for (auto& btn : toggleButtons_) {
        if (btn.textureID != 0) {
            glDeleteTextures(1, &btn.textureID);
            btn.textureID = 0;
        }
    }
}

void SignalVisualizer::cleanup() {
    cleanupButtonTextures();

    if (originalVAO_ != 0) glDeleteVertexArrays(1, &originalVAO_);
    if (originalVBO_ != 0) glDeleteBuffers(1, &originalVBO_);
    if (noisyVAO_    != 0) glDeleteVertexArrays(1, &noisyVAO_);
    if (noisyVBO_    != 0) glDeleteBuffers(1, &noisyVBO_);
    if (filteredVAO_ != 0) glDeleteVertexArrays(1, &filteredVAO_);
    if (filteredVBO_ != 0) glDeleteBuffers(1, &filteredVBO_);

    if (shaderProgram_        != 0) glDeleteProgram(shaderProgram_);
    if (textureShaderProgram_ != 0) glDeleteProgram(textureShaderProgram_);

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}
