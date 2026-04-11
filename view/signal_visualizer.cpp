#include "signal_visualizer.h"
#include "stb_image.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef ROOT_PATH
#define ROOT_PATH "."
#endif

static const std::string IMG_DIR    = std::string(ROOT_PATH) + "/view/images/";
static const std::string SHADER_DIR = std::string(ROOT_PATH) + "/view/shaders/";

// ── Загрузка исходника шейдера из файла ──────────────────────────────────────
static std::string loadShaderSource(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть шейдер: " << path << std::endl;
        return {};
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// ═════════════════════════════════════════════════════════════════════════════
// Конструктор / деструктор
// ═════════════════════════════════════════════════════════════════════════════

SignalVisualizer::SignalVisualizer(int width, int height, const std::string& title)
    : window_(nullptr)
    , windowWidth_(width)
    , windowHeight_(height)
    , logicalWidth_(width)
    , logicalHeight_(height)
    , windowTitle_(title)
    , minY_(-2.0f), maxY_(2.0f)
    , autoScale_(true)
    , specMinY_(-80.0f), specMaxY_(0.0f)
    , zoomFactor_(1.0f)
    , offsetX_(0.0f), offsetY_(0.0f)
    , minZoom_(0.1f), maxZoom_(10.0f)
    , showOriginal_(true), showNoisy_(true), showFiltered_(true)
    , showSpecBefore_(true), showSpecAfter_(true), showSpecDiff_(true)
    , splitView_(false), splitRatio_(0.6f)
    , shaderProgram_(0)
    , originalVAO_(0), originalVBO_(0)
    , noisyVAO_(0),    noisyVBO_(0)
    , filteredVAO_(0), filteredVBO_(0)
    , specBeforeVAO_(0), specBeforeVBO_(0)
    , specAfterVAO_(0),  specAfterVBO_(0)
    , specDiffVAO_(0),   specDiffVBO_(0)
    , textureShaderProgram_(0)
    , originalColor_(0.0f, 0.8f, 0.0f)
    , noisyColor_(0.8f, 0.0f, 0.0f)
    , filteredColor_(0.0f, 0.4f, 0.9f)
    , specBeforeColor_(0.9f, 0.3f, 0.3f)
    , specAfterColor_(0.3f, 0.5f, 1.0f)
    , specDiffColor_(1.0f, 0.85f, 0.0f)
{
}

SignalVisualizer::~SignalVisualizer()
{
    cleanup();
}

// ═════════════════════════════════════════════════════════════════════════════
// Инициализация
// ═════════════════════════════════════════════════════════════════════════════

bool SignalVisualizer::initialize()
{
    if (!initializeGLFW()) return false;
    if (!initializeGLEW()) { cleanup(); return false; }
    if (!createShaderProgram()) { cleanup(); return false; }
    if (!createTextureShaderProgram()) { cleanup(); return false; }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.5f);

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);

    std::cout << "OpenGL инициализирован успешно" << std::endl;
    return true;
}

bool SignalVisualizer::initializeGLFW()
{
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
    window_ = glfwCreateWindow(windowWidth_, windowHeight_, windowTitle_.c_str(),
                               nullptr, nullptr);
    if (!window_) {
        std::cerr << "Ошибка создания окна GLFW" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    // Логический размер окна — для hit-test курсора (glfwGetCursorPos возвращает логические px)
    glfwGetWindowSize(window_, &logicalWidth_, &logicalHeight_);
    // Физический размер framebuffer — для glViewport (Retina = 2x логического)
    glfwGetFramebufferSize(window_, &windowWidth_, &windowHeight_);
    return true;
}

bool SignalVisualizer::initializeGLEW()
{
    if (glewInit() != GLEW_OK) {
        std::cerr << "Ошибка инициализации GLEW" << std::endl;
        return false;
    }
    std::cout << "OpenGL версия: " << glGetString(GL_VERSION) << std::endl;
    return true;
}

bool SignalVisualizer::createShaderProgram()
{
    std::string vsSource = loadShaderSource(SHADER_DIR + "signal.vert");
    std::string fsSource = loadShaderSource(SHADER_DIR + "signal.frag");
    if (vsSource.empty() || fsSource.empty()) return false;

    GLuint vs = compileShader(vsSource, GL_VERTEX_SHADER);
    if (!vs) return false;
    GLuint fs = compileShader(fsSource, GL_FRAGMENT_SHADER);
    if (!fs) { glDeleteShader(vs); return false; }

    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vs);
    glAttachShader(shaderProgram_, fs);
    glLinkProgram(shaderProgram_);

    GLint ok; GLchar log[512];
    glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(shaderProgram_, 512, nullptr, log);
        std::cerr << "Ошибка линковки шейдера: " << log << std::endl;
        glDeleteShader(vs); glDeleteShader(fs);
        return false;
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return true;
}

bool SignalVisualizer::createTextureShaderProgram()
{
    std::string vsSource = loadShaderSource(SHADER_DIR + "button.vert");
    std::string fsSource = loadShaderSource(SHADER_DIR + "button.frag");
    if (vsSource.empty() || fsSource.empty()) return false;

    GLuint vs = compileShader(vsSource, GL_VERTEX_SHADER);
    if (!vs) return false;
    GLuint fs = compileShader(fsSource, GL_FRAGMENT_SHADER);
    if (!fs) { glDeleteShader(vs); return false; }

    textureShaderProgram_ = glCreateProgram();
    glAttachShader(textureShaderProgram_, vs);
    glAttachShader(textureShaderProgram_, fs);
    glLinkProgram(textureShaderProgram_);

    GLint ok; GLchar log[512];
    glGetProgramiv(textureShaderProgram_, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(textureShaderProgram_, 512, nullptr, log);
        std::cerr << "Ошибка линковки текстурного шейдера: " << log << std::endl;
        glDeleteShader(vs); glDeleteShader(fs);
        return false;
    }
    glDeleteShader(vs); glDeleteShader(fs);
    glUseProgram(textureShaderProgram_);
    glUniform1i(glGetUniformLocation(textureShaderProgram_, "texSampler"), 0);
    return true;
}

GLuint SignalVisualizer::compileShader(const std::string& source, GLenum type)  // NOLINT
{
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok; GLchar log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Ошибка компиляции шейдера: " << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint SignalVisualizer::loadTexture(const std::string& path)
{
    stbi_set_flip_vertically_on_load(true);
    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "Загружена текстура: " << path << " [" << w << "x" << h << "]\n";
    return texID;
}

// ═════════════════════════════════════════════════════════════════════════════
// Данные
// ═════════════════════════════════════════════════════════════════════════════

void SignalVisualizer::setSignalData(const SignalProcessor::Signal& noisy,
                                     const SignalProcessor::Signal& filtered,
                                     const SignalProcessor::Signal& original)
{
    noisySignal_    = noisy;
    filteredSignal_ = filtered;
    originalSignal_ = original;

    if (autoScale_) calculateAutoScale();
    updateSignalBuffers();
    initializeToggleButtons();
}

void SignalVisualizer::enableSplitView(const SignalProcessor::Signal& specBefore,
                                       const SignalProcessor::Signal& specAfter,
                                       const SignalProcessor::Signal& specDiff,
                                       float ratio)
{
    specBefore_ = specBefore;
    specAfter_  = specAfter;
    specDiff_   = specDiff;
    splitRatio_ = std::max(0.3f, std::min(0.8f, ratio));
    splitView_  = true;

    calculateSpectrumScale();
    updateSpectrumBuffers();
    initializeSpecButtons();
}

void SignalVisualizer::disableSplitView()
{
    splitView_ = false;
    specButtons_.clear();
}

// ─── Масштабирование ─────────────────────────────────────────────────────────

void SignalVisualizer::calculateAutoScale()
{
    if (noisySignal_.empty() && filteredSignal_.empty()) {
        minY_ = -2.0f; maxY_ = 2.0f; return;
    }
    double mn =  1e30, mx = -1e30;
    for (const auto& sig : {noisySignal_, filteredSignal_, originalSignal_}) {
        if (!sig.empty()) {
            for (double v : sig) { mn = std::min(mn, v); mx = std::max(mx, v); }
        }
    }
    double range = mx - mn;
    double pad   = range * 0.1;
    minY_ = static_cast<float>(mn - pad);
    maxY_ = static_cast<float>(mx + pad);
    if (std::abs(maxY_ - minY_) < 1e-6f) { minY_ -= 1.0f; maxY_ += 1.0f; }
}

void SignalVisualizer::calculateSpectrumScale()
{
    double mn =  1e30, mx = -1e30;
    for (const auto& sig : {specBefore_, specAfter_, specDiff_}) {
        if (!sig.empty()) {
            for (double v : sig) { mn = std::min(mn, v); mx = std::max(mx, v); }
        }
    }
    if (mn > mx) { specMinY_ = -80.0f; specMaxY_ = 0.0f; return; }
    double range = mx - mn;
    double pad   = range * 0.08;
    specMinY_ = static_cast<float>(mn - pad);
    specMaxY_ = static_cast<float>(mx + pad);
    if (std::abs(specMaxY_ - specMinY_) < 1e-3f) { specMinY_ -= 10.0f; specMaxY_ += 10.0f; }
}

// ─── Буферы ───────────────────────────────────────────────────────────────────

float SignalVisualizer::indexToX(size_t index, size_t signalLength) const
{
    if (signalLength <= 1) return offsetX_;
    float nx = -1.0f + (2.0f * index) / static_cast<float>(signalLength - 1);
    return (nx * zoomFactor_) + offsetX_;
}

float SignalVisualizer::valueToY(double value, float yMin, float yMax) const
{
    if (std::abs(yMax - yMin) < 1e-9f) return 0.0f;
    float ny = -1.0f + 2.0f * static_cast<float>(value - yMin) / (yMax - yMin);
    return (ny * zoomFactor_) + offsetY_;
}

/**
 * Заполняет VAO/VBO для одной кривой.
 * yMin/yMax — диапазон значений панели, в которой она рисуется.
 */
void SignalVisualizer::createSignalBuffers(const SignalProcessor::Signal& signal,
                                           GLuint& vao, GLuint& vbo,
                                           float yMin, float yMax,
                                           float /*xMin*/, float /*xMax*/)
{
    if (signal.empty()) return;

    std::vector<float> verts;
    verts.reserve(signal.size() * 2);
    for (size_t i = 0; i < signal.size(); ++i) {
        verts.push_back(indexToX(i, signal.size()));
        verts.push_back(valueToY(signal[i], yMin, yMax));
    }

    if (vao == 0) { glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void SignalVisualizer::updateSignalBuffers()
{
    createSignalBuffers(originalSignal_, originalVAO_, originalVBO_, minY_, maxY_);
    createSignalBuffers(noisySignal_,    noisyVAO_,    noisyVBO_,    minY_, maxY_);
    createSignalBuffers(filteredSignal_, filteredVAO_, filteredVBO_, minY_, maxY_);
}

void SignalVisualizer::updateSpectrumBuffers()
{
    createSignalBuffers(specBefore_, specBeforeVAO_, specBeforeVBO_,
                        specMinY_, specMaxY_);
    createSignalBuffers(specAfter_,  specAfterVAO_,  specAfterVBO_,
                        specMinY_, specMaxY_);
    createSignalBuffers(specDiff_,   specDiffVAO_,   specDiffVBO_,
                        specMinY_, specMaxY_);
}

// ═════════════════════════════════════════════════════════════════════════════
// Основной цикл / render
// ═════════════════════════════════════════════════════════════════════════════

void SignalVisualizer::run()
{
    std::cout << "Запуск визуализации...\n";
    std::cout << "  ESC/Q    — выход\n";
    std::cout << "  SPACE    — сброс вида\n";
    std::cout << "  +/-      — зум\n";
    std::cout << "  стрелки  — панорамирование\n";
    std::cout << "  G/N/F    — чистый / зашумлённый / фильтрованный (время)\n";
    if (splitView_)
        std::cout << "  1/2/3    — спектр до / после / разность НИП\n";

    while (!shouldClose()) {
        processEvents();
        render();
        glfwSwapBuffers(window_);
    }
}

bool SignalVisualizer::shouldClose() const
{
    return glfwWindowShouldClose(window_);
}

void SignalVisualizer::processEvents()
{
    glfwPollEvents();
}

// ─── Отрисовка ────────────────────────────────────────────────────────────────

void SignalVisualizer::render()
{
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (splitView_) {
        drawTopPanel();
        drawBottomPanel();
    } else {
        // Обычный режим — вся область как раньше
        glViewport(0, 0, windowWidth_, windowHeight_);
        glUseProgram(shaderProgram_);

        // Простая сетка для всего окна
        drawGrid(0, 0, static_cast<float>(windowWidth_),
                      static_cast<float>(windowHeight_));
        drawAxes(0, 0, static_cast<float>(windowWidth_),
                      static_cast<float>(windowHeight_));

        if (!originalSignal_.empty() && showOriginal_)
            drawSignal(originalVAO_, originalSignal_.size(), originalColor_);
        if (!noisySignal_.empty() && showNoisy_)
            drawSignal(noisyVAO_, noisySignal_.size(), noisyColor_);
        if (!filteredSignal_.empty() && showFiltered_)
            drawSignal(filteredVAO_, filteredSignal_.size(), filteredColor_);

        drawToggleButtons();
    }
}

// ─── Верхняя панель (временной сигнал) ───────────────────────────────────────

void SignalVisualizer::drawTopPanel()
{
    const int topH = static_cast<int>(windowHeight_ * splitRatio_);
    const int botY = windowHeight_ - topH;   // нижний Y-пиксель верхней панели

    glViewport(0, botY, windowWidth_, topH);
    glUseProgram(shaderProgram_);
    drawGrid(0, 0, static_cast<float>(windowWidth_), static_cast<float>(topH));
    drawAxes(0, 0, static_cast<float>(windowWidth_), static_cast<float>(topH));

    if (!originalSignal_.empty() && showOriginal_)
        drawSignal(originalVAO_, originalSignal_.size(), originalColor_);
    if (!noisySignal_.empty() && showNoisy_)
        drawSignal(noisyVAO_, noisySignal_.size(), noisyColor_);
    if (!filteredSignal_.empty() && showFiltered_)
        drawSignal(filteredVAO_, filteredSignal_.size(), filteredColor_);

    // Метка панели
    // (текстовый рендеринг не реализован в OpenGL-3.3 core без FreeType;
    //  панели различаются визуально по цветам кривых и кнопкам)

    drawToggleButtons();
}

// ─── Нижняя панель (доплеровский спектр) ─────────────────────────────────────

void SignalVisualizer::drawBottomPanel()
{
    const int topH = static_cast<int>(windowHeight_ * splitRatio_);
    const int botH = windowHeight_ - topH;

    glViewport(0, 0, windowWidth_, botH);
    glUseProgram(shaderProgram_);

    // Тёмный фон нижней панели
    {
        float bg[] = { -1,-1, 1,-1, -1,1, 1,1 };
        GLuint vao, vbo;
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(bg), bg, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                    0.08f, 0.08f, 0.12f);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDeleteVertexArrays(1, &vao); glDeleteBuffers(1, &vbo);
    }

    // Разделитель между панелями (белая горизонтальная линия сверху botH)
    {
        float sep[] = { -1.0f, 0.995f,  1.0f, 0.995f };
        GLuint vao, vbo;
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(sep), sep, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                    0.5f, 0.5f, 0.5f);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, 2);
        glDeleteVertexArrays(1, &vao); glDeleteBuffers(1, &vbo);
        glLineWidth(1.5f);
    }

    drawGrid(0, 0, static_cast<float>(windowWidth_), static_cast<float>(botH));
    drawAxes(0, 0, static_cast<float>(windowWidth_), static_cast<float>(botH));

    if (!specBefore_.empty() && showSpecBefore_)
        drawSignal(specBeforeVAO_, specBefore_.size(), specBeforeColor_);
    if (!specAfter_.empty() && showSpecAfter_)
        drawSignal(specAfterVAO_, specAfter_.size(), specAfterColor_);
    if (!specDiff_.empty() && showSpecDiff_)
        drawSignal(specDiffVAO_, specDiff_.size(), specDiffColor_);

    drawSpecButtons();
}

// ─── Рисование одной кривой ───────────────────────────────────────────────────

void SignalVisualizer::drawSignal(GLuint vao, size_t cnt, const Color& color)
{
    if (!vao || !cnt) return;
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                color.r, color.g, color.b);
    glBindVertexArray(vao);
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(cnt));
    glBindVertexArray(0);
}

// ─── Сетка и оси ─────────────────────────────────────────────────────────────

void SignalVisualizer::drawGrid(float /*vpX*/, float /*vpY*/,
                                float /*vpW*/, float /*vpH*/)
{
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                0.22f, 0.22f, 0.28f);

    auto line = [](float x0, float y0, float x1, float y1) {
        float v[] = {x0,y0,x1,y1};
        GLuint a,b;
        glGenVertexArrays(1,&a); glGenBuffers(1,&b);
        glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER,b);
        glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES,0,2);
        glDeleteVertexArrays(1,&a); glDeleteBuffers(1,&b);
    };
    for (int i = -8; i <= 8; i += 2) {
        float t = i / 10.0f;
        line(t, -1.0f, t, 1.0f);
        line(-1.0f, t, 1.0f, t);
    }
}

void SignalVisualizer::drawAxes(float /*vpX*/, float /*vpY*/,
                                float /*vpW*/, float /*vpH*/)
{
    glUniform3f(glGetUniformLocation(shaderProgram_, "color"),
                0.5f, 0.5f, 0.55f);
    glLineWidth(2.0f);

    auto line = [](float x0, float y0, float x1, float y1) {
        float v[] = {x0,y0,x1,y1};
        GLuint a,b;
        glGenVertexArrays(1,&a); glGenBuffers(1,&b);
        glBindVertexArray(a); glBindBuffer(GL_ARRAY_BUFFER,b);
        glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES,0,2);
        glDeleteVertexArrays(1,&a); glDeleteBuffers(1,&b);
    };
    line(-1.0f, 0.0f, 1.0f, 0.0f);
    line(0.0f, -1.0f, 0.0f, 1.0f);
    glLineWidth(1.5f);
}

// ═════════════════════════════════════════════════════════════════════════════
// Кнопки переключения (временная панель)
// ═════════════════════════════════════════════════════════════════════════════

void SignalVisualizer::initializeToggleButtons()
{
    cleanupButtonTextures();
    toggleButtons_.clear();

    float halfW = 0.12f, halfH = 0.065f;
    float startX = -0.82f, startY = 0.88f, spacing = 0.30f;

    struct Info { float cx; const char* img; Color col; bool* flag; };
    Info infos[] = {
        { startX,           "btn_clean.png",    originalColor_, &showOriginal_ },
        { startX+spacing,   "btn_noisy.png",    noisyColor_,    &showNoisy_    },
        { startX+2*spacing, "btn_filtered.png", filteredColor_, &showFiltered_ },
    };
    for (auto& i : infos) {
        Button btn(i.cx, startY, halfW, halfH, i.col, i.flag);
        btn.textureID = loadTexture(IMG_DIR + i.img);
        toggleButtons_.push_back(std::move(btn));
    }
}

void SignalVisualizer::initializeSpecButtons()
{
    // Освобождаем только спектральные текстуры
    for (auto& btn : specButtons_)
        if (btn.textureID) { glDeleteTextures(1, &btn.textureID); btn.textureID = 0; }
    specButtons_.clear();

    float halfW = 0.12f, halfH = 0.065f;
    float startX = -0.82f, startY = 0.88f, spacing = 0.30f;

    struct Info { float cx; const char* img; Color col; bool* flag; };
    Info infos[] = {
        { startX,           "btn_spec_before.png", specBeforeColor_, &showSpecBefore_ },
        { startX+spacing,   "btn_spec_after.png",  specAfterColor_,  &showSpecAfter_  },
        { startX+2*spacing, "btn_spec_diff.png",   specDiffColor_,   &showSpecDiff_   },
    };
    for (auto& i : infos) {
        Button btn(i.cx, startY, halfW, halfH, i.col, i.flag);
        btn.textureID = loadTexture(IMG_DIR + i.img);
        specButtons_.push_back(std::move(btn));
    }
}

void SignalVisualizer::drawToggleButtons()
{
    for (const auto& b : toggleButtons_) drawRectButton(b);
}

void SignalVisualizer::drawSpecButtons()
{
    for (const auto& b : specButtons_) drawRectButton(b);
}

void SignalVisualizer::drawRectButton(const Button& button)
{
    float x0 = button.centerX - button.halfW;
    float x1 = button.centerX + button.halfW;
    float y0 = button.centerY - button.halfH;
    float y1 = button.centerY + button.halfH;
    bool active = *(button.visibility);

    if (button.textureID) {
        float verts[] = { x0,y0,0,0,  x1,y0,1,0,  x0,y1,0,1,  x1,y1,1,1 };
        GLuint vao, vbo;
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
        glEnableVertexAttribArray(1);

        glUseProgram(textureShaderProgram_);
        glUniform1f(glGetUniformLocation(textureShaderProgram_,"dimFactor"),
                    active ? 1.0f : 0.4f);
        glUniform1f(glGetUniformLocation(textureShaderProgram_,"hoverGlow"),
                    button.isHovered ? 1.0f : 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, button.textureID);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo);
        glUseProgram(shaderProgram_);
    } else {
        Color fc = button.color;
        if (!active) { fc.r*=0.3f; fc.g*=0.3f; fc.b*=0.3f; }
        glUniform3f(glGetUniformLocation(shaderProgram_,"color"), fc.r, fc.g, fc.b);
        float fv[] = {x0,y0, x1,y0, x0,y1, x1,y1};
        GLuint vao, vbo;
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fv), fv, GL_STATIC_DRAW);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo);
    }

    // Контур кнопки
    glUseProgram(shaderProgram_);
    if (button.isHovered) {
        glUniform3f(glGetUniformLocation(shaderProgram_,"color"), 1,1,1);
        glLineWidth(3.0f);
    } else {
        float br = active ? 1.0f : 0.4f;
        glUniform3f(glGetUniformLocation(shaderProgram_,"color"), br,br,br);
        glLineWidth(active ? 2.0f : 1.0f);
    }
    float bv[] = {x0,y0, x1,y0, x1,y1, x0,y1};
    GLuint bvao, bvbo;
    glGenVertexArrays(1,&bvao); glGenBuffers(1,&bvbo);
    glBindVertexArray(bvao); glBindBuffer(GL_ARRAY_BUFFER,bvbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(bv),bv,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINE_LOOP,0,4);
    glDeleteVertexArrays(1,&bvao); glDeleteBuffers(1,&bvbo);
    glLineWidth(1.5f);
}

// ═════════════════════════════════════════════════════════════════════════════
// Колбэки GLFW
// ═════════════════════════════════════════════════════════════════════════════

void SignalVisualizer::framebufferSizeCallback(GLFWwindow* window, int w, int h)
{
    glViewport(0, 0, w, h);
    auto* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (vis) {
        vis->windowWidth_  = w;
        vis->windowHeight_ = h;
        // Обновляем логический размер (для hit-test курсора)
        glfwGetWindowSize(window, &vis->logicalWidth_, &vis->logicalHeight_);
    }
}

void SignalVisualizer::keyCallback(GLFWwindow* window, int key, int /*sc*/,
                                   int action, int /*mods*/)
{
    auto* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!vis || action != GLFW_PRESS) return;

    switch (key) {
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        case GLFW_KEY_R: vis->updateSignalBuffers(); break;
        case GLFW_KEY_SPACE: vis->resetView(); break;
        case GLFW_KEY_UP:    vis->pan(0.0f, -0.1f); break;
        case GLFW_KEY_DOWN:  vis->pan(0.0f,  0.1f); break;
        case GLFW_KEY_LEFT:  vis->pan( 0.1f, 0.0f); break;
        case GLFW_KEY_RIGHT: vis->pan(-0.1f, 0.0f); break;
        case GLFW_KEY_EQUAL:
        case GLFW_KEY_KP_ADD:      vis->zoom(1.2f); break;
        case GLFW_KEY_MINUS:
        case GLFW_KEY_KP_SUBTRACT: vis->zoom(0.8f); break;
        // Временная панель
        case GLFW_KEY_G:
            vis->showOriginal_ = !vis->showOriginal_;
            std::cout << "Чистый: " << (vis->showOriginal_ ? "показан" : "скрыт") << "\n"; break;
        case GLFW_KEY_N:
            vis->showNoisy_ = !vis->showNoisy_;
            std::cout << "С НИП: " << (vis->showNoisy_ ? "показан" : "скрыт") << "\n"; break;
        case GLFW_KEY_F:
            vis->showFiltered_ = !vis->showFiltered_;
            std::cout << "Компенсирован: " << (vis->showFiltered_ ? "показан" : "скрыт") << "\n"; break;
        // Спектральная панель
        case GLFW_KEY_1:
            if (vis->splitView_) {
                vis->showSpecBefore_ = !vis->showSpecBefore_;
                std::cout << "Спектр до: " << (vis->showSpecBefore_ ? "показан" : "скрыт") << "\n";
            } break;
        case GLFW_KEY_2:
            if (vis->splitView_) {
                vis->showSpecAfter_ = !vis->showSpecAfter_;
                std::cout << "Спектр после: " << (vis->showSpecAfter_ ? "показан" : "скрыт") << "\n";
            } break;
        case GLFW_KEY_3:
            if (vis->splitView_) {
                vis->showSpecDiff_ = !vis->showSpecDiff_;
                std::cout << "Разность НИП: " << (vis->showSpecDiff_ ? "показан" : "скрыт") << "\n";
            } break;
    }
}

void SignalVisualizer::scrollCallback(GLFWwindow* window, double /*x*/, double y)
{
    auto* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (vis) vis->zoom(1.0f + static_cast<float>(y) * 0.1f);
}

void SignalVisualizer::mouseButtonCallback(GLFWwindow* window, int btn,
                                          int action, int /*mods*/)
{
    auto* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (!vis) return;
    if (btn == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        vis->handleMouseClick(x, y);
    }
}

void SignalVisualizer::cursorPosCallback(GLFWwindow* window, double x, double y)
{
    auto* vis = static_cast<SignalVisualizer*>(glfwGetWindowUserPointer(window));
    if (vis) vis->updateHoverState(x, y);
}

// ─── Зум / пан ────────────────────────────────────────────────────────────────

void SignalVisualizer::zoom(float factor)
{
    zoomFactor_ = std::max(minZoom_, std::min(maxZoom_, zoomFactor_ * factor));
    updateSignalBuffers();
    if (splitView_) updateSpectrumBuffers();
}

void SignalVisualizer::pan(float dx, float dy)
{
    offsetX_ += dx / zoomFactor_;
    offsetY_ += dy / zoomFactor_;
    updateSignalBuffers();
    if (splitView_) updateSpectrumBuffers();
}

void SignalVisualizer::resetView()
{
    zoomFactor_ = 1.0f; offsetX_ = 0.0f; offsetY_ = 0.0f;
    updateSignalBuffers();
    if (splitView_) updateSpectrumBuffers();
    std::cout << "Вид сброшен\n";
}

void SignalVisualizer::updateViewTransform()
{
    updateSignalBuffers();
    if (splitView_) updateSpectrumBuffers();
}

// ─── Hover / click ────────────────────────────────────────────────────────────

bool SignalVisualizer::isPointInButton(double x, double y, const Button& btn) const
{
    // glfwGetCursorPos возвращает логические координаты — делим на логический размер окна
    float nx = static_cast<float>((2.0 * x) / logicalWidth_  - 1.0);
    float ny = static_cast<float>(1.0 - (2.0 * y) / logicalHeight_);
    return (nx >= btn.centerX - btn.halfW && nx <= btn.centerX + btn.halfW &&
            ny >= btn.centerY - btn.halfH && ny <= btn.centerY + btn.halfH);
}

// Проверка попадания точки (x,y) в кнопку спектральной панели.
// Кнопки нарисованы в NDC нижней панели — пересчитываем y относительно логического размера.
bool SignalVisualizer::isPointInSpecButton(double x, double y,
                                           const Button& btn) const
{
    // Высота нижней панели в логических пикселях
    const int botHLogical = logicalHeight_ - static_cast<int>(logicalHeight_ * splitRatio_);
    if (botHLogical <= 0) return false;

    float nx = static_cast<float>((2.0 * x) / logicalWidth_ - 1.0);
    // Нижняя панель: y=0..botHLogical снизу экрана
    float yPanelBot = static_cast<float>(logicalHeight_) - y;
    float nyPanel = -1.0f + 2.0f * static_cast<float>(yPanelBot) / static_cast<float>(botHLogical);

    return (nx >= btn.centerX - btn.halfW && nx <= btn.centerX + btn.halfW &&
            nyPanel >= btn.centerY - btn.halfH && nyPanel <= btn.centerY + btn.halfH);
}

void SignalVisualizer::updateHoverState(double x, double y)
{
    for (auto& b : toggleButtons_) b.isHovered = isPointInButton(x, y, b);

    if (splitView_) {
        for (auto& b : specButtons_)
            b.isHovered = isPointInSpecButton(x, y, b);
    }
}

void SignalVisualizer::handleMouseClick(double x, double y)
{
    for (const auto& b : toggleButtons_) {
        if (isPointInButton(x, y, b)) {
            *(b.visibility) = !*(b.visibility);
            return;
        }
    }
    if (splitView_) {
        for (const auto& b : specButtons_) {
            if (isPointInSpecButton(x, y, b)) {
                *(b.visibility) = !*(b.visibility);
                return;
            }
        }
    }
}

// ─── Освобождение ────────────────────────────────────────────────────────────

void SignalVisualizer::cleanupButtonTextures()
{
    for (auto& b : toggleButtons_)
        if (b.textureID) { glDeleteTextures(1, &b.textureID); b.textureID = 0; }
    for (auto& b : specButtons_)
        if (b.textureID) { glDeleteTextures(1, &b.textureID); b.textureID = 0; }
}

void SignalVisualizer::cleanup()
{
    cleanupButtonTextures();

    auto del = [](GLuint& vao, GLuint& vbo) {
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
        if (vbo) { glDeleteBuffers(1,  &vbo);     vbo = 0; }
    };
    del(originalVAO_,  originalVBO_);
    del(noisyVAO_,     noisyVBO_);
    del(filteredVAO_,  filteredVBO_);
    del(specBeforeVAO_,specBeforeVBO_);
    del(specAfterVAO_, specAfterVBO_);
    del(specDiffVAO_,  specDiffVBO_);

    if (shaderProgram_)        { glDeleteProgram(shaderProgram_);        shaderProgram_        = 0; }
    if (textureShaderProgram_) { glDeleteProgram(textureShaderProgram_); textureShaderProgram_ = 0; }

    if (window_) { glfwDestroyWindow(window_); window_ = nullptr; }
    glfwTerminate();
}
