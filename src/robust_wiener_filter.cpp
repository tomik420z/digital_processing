#include "robust_wiener_filter.h"
#include "utils/linear_system_solver.h"
#include "utils/median.h"
#include "utils/fft.h"

#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <numeric>

// ─────────────────────────────────────────────────────────────────────────────
// Конструктор / setParameters
// ─────────────────────────────────────────────────────────────────────────────

RobustWienerFilter::RobustWienerFilter(size_t filterOrder,
                                       size_t desiredWindow,
                                       double regularization,
                                       double outlierThreshold,
                                       size_t outlierWindow)
    : filterOrder_(filterOrder),
      desiredWindow_(desiredWindow),
      regularization_(regularization),
      outlierThreshold_(outlierThreshold),
      outlierWindow_(outlierWindow)
{
    if (filterOrder_ == 0)
        throw std::invalid_argument("RobustWienerFilter: filterOrder must be > 0");
    if (desiredWindow_ == 0)
        throw std::invalid_argument("RobustWienerFilter: desiredWindow must be > 0");
    if (regularization_ < 0.0)
        throw std::invalid_argument("RobustWienerFilter: regularization must be >= 0");
    if (outlierThreshold_ <= 0.0)
        throw std::invalid_argument("RobustWienerFilter: outlierThreshold must be > 0");
    // outlierWindow должно быть нечётным и > 0
    if (outlierWindow_ == 0)
        outlierWindow_ = 1;
    if (outlierWindow_ % 2 == 0)
        ++outlierWindow_; // делаем нечётным

    weights_.resize(filterOrder_, 0.0);
}

void RobustWienerFilter::setParameters(size_t filterOrder,
                                        size_t desiredWindow,
                                        double regularization,
                                        double outlierThreshold,
                                        size_t outlierWindow)
{
    if (filterOrder == 0)
        throw std::invalid_argument("RobustWienerFilter: filterOrder must be > 0");
    if (desiredWindow == 0)
        throw std::invalid_argument("RobustWienerFilter: desiredWindow must be > 0");
    if (regularization < 0.0)
        throw std::invalid_argument("RobustWienerFilter: regularization must be >= 0");
    if (outlierThreshold <= 0.0)
        throw std::invalid_argument("RobustWienerFilter: outlierThreshold must be > 0");

    filterOrder_      = filterOrder;
    desiredWindow_    = desiredWindow;
    regularization_   = regularization;
    outlierThreshold_ = outlierThreshold;
    outlierWindow_    = (outlierWindow == 0) ? 1 : outlierWindow;
    if (outlierWindow_ % 2 == 0) ++outlierWindow_;

    weights_.resize(filterOrder_, 0.0);
}

std::string RobustWienerFilter::getName() const
{
    return "RobustWienerFilter_ord" + std::to_string(filterOrder_) +
           "_win" + std::to_string(desiredWindow_) +
           "_thr" + std::to_string(static_cast<int>(outlierThreshold_ * 10));
}

std::vector<double> RobustWienerFilter::getWeights() const
{
    return std::vector<double>(weights_.begin(), weights_.end());
}

// ─────────────────────────────────────────────────────────────────────────────
// Комбинированная авто-настройка параметров (A: статистика + B: спектр)
// ─────────────────────────────────────────────────────────────────────────────

WienerParams RobustWienerFilter::estimateParameters(const std::vector<double>& signal)
{
    WienerParams p{};
    const size_t N = signal.size();

    if (N < 8) {
        // Слишком короткий сигнал — возвращаем безопасные значения по умолчанию
        p.filterOrder      = 4;
        p.desiredWindow    = 3;
        p.regularization   = 1e-4;
        p.outlierThreshold = 3.5;
        p.outlierWindow    = 9;
        return p;
    }

    // ── Вариант A: статистический анализ ─────────────────────────────────────

    // 1. RMS входного сигнала
    double sumSq = 0.0;
    for (double v : signal) sumSq += v * v;
    p.estimatedSignalRMS = std::sqrt(sumSq / static_cast<double>(N));

    // 2. MAD (медиана абсолютных отклонений от медианы) → оценка σ шума
    std::vector<double> buf(signal);
    std::nth_element(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(N / 2), buf.end());
    const double med = buf[N / 2];
    for (double& v : buf) v = std::abs(v - med);
    std::nth_element(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(N / 2), buf.end());
    const double madVal = buf[N / 2];
    p.estimatedNoiseSigma = madVal / 0.6745; // оценка σ для гауссова шума

    // 3. SNR в дБ
    const double snr = (p.estimatedNoiseSigma > 1e-12)
        ? (p.estimatedSignalRMS / p.estimatedNoiseSigma)
        : 1000.0;
    p.estimatedSNR_dB = 20.0 * std::log10(snr);

    // 4. outlierThreshold: чем меньше SNR — тем ниже порог (детектор чувствительнее)
    if (p.estimatedSNR_dB < 5.0)
        p.outlierThreshold = 2.5;   // шумный сигнал — ловим даже небольшие выбросы
    else if (p.estimatedSNR_dB < 15.0)
        p.outlierThreshold = 3.5;   // умеренный шум — стандартный порог
    else
        p.outlierThreshold = 5.0;   // чистый сигнал — только явные выбросы

    // 5. regularization масштабируется с дисперсией шума
    p.regularization = p.estimatedNoiseSigma * p.estimatedNoiseSigma;
    if (p.regularization < 1e-9) p.regularization = 1e-4; // минимальная регуляризация
    if (p.regularization > 1e-1) p.regularization = 1e-1; // макисмальная регуляризация
    // ── Вариант B: спектральный анализ для подбора порядка фильтра ───────────

    // 6. FFT входного сигнала
    CVector spectrum = fft(signal);
    const size_t fftLen = spectrum.size(); // степень двойки >= N
    const size_t halfLen = fftLen / 2;     // только положительные частоты

    // 7. Спектр мощности — только положительные частоты [0 .. halfLen-1]
    //    Нормированная частота k-го бина: f_k = k / fftLen  (диапазон 0..0.5)
    std::vector<double> powerSpectrum(halfLen);
    double totalPower = 0.0;
    for (size_t k = 0; k < halfLen; ++k) {
        powerSpectrum[k] = std::norm(spectrum[k]); // |X[k]|²
        totalPower += powerSpectrum[k];
    }

    // 8. Найти доминирующую частоту (бин с максимальной мощностью, кроме DC)
    size_t dominantBin = 1;
    double maxPow = 0.0;
    for (size_t k = 1; k < halfLen; ++k) {
        if (powerSpectrum[k] > maxPow) {
            maxPow = powerSpectrum[k];
            dominantBin = k;
        }
    }
    p.dominantFrequency = static_cast<double>(dominantBin) / static_cast<double>(fftLen);

    // 9. Найти f_95 — нормированная частота, ниже которой 95% мощности
    //    Используется для оценки "ширины полосы" полезного сигнала
    double cumPow = 0.0;
    const double threshold95 = 0.95 * totalPower;
    size_t bin95 = halfLen - 1;
    for (size_t k = 0; k < halfLen; ++k) {
        cumPow += powerSpectrum[k];
        if (cumPow >= threshold95) {
            bin95 = k;
            break;
        }
    }
    const double f95 = static_cast<double>(bin95 + 1) / static_cast<double>(fftLen);

    // 10. filterOrder = round(0.75 / f_95): компромисс между «памятью» и переподгонкой.
    //     • f_95 — частота, ниже которой 95% мощности спектра
    //     • 0.75/f_95 ≈ 3/4 периода: достаточно для захвата формы сигнала,
    //       но не так велико, чтобы начать «запоминать» шум
    //     Ограничиваем диапазоном [8, 256]
    const double rawOrder = 0.75 / std::max(f95, 0.003); // защита от нуля
    const size_t rawOrderInt = static_cast<size_t>(std::round(rawOrder));
    p.filterOrder = std::clamp(rawOrderInt, size_t(8), size_t(256));

    // 11. desiredWindow ≈ filterOrder * 2 / 5, нечётное, диапазон [5, 127]
    //     Примерно 40% от порядка фильтра — достаточно широкое окно медианы,
    //     чтобы надёжно оценить медленный тренд, но не сглаживать детали
    size_t dw = std::max(size_t(5), p.filterOrder * 2 / 5);
    if (dw % 2 == 0) ++dw;
    p.desiredWindow = std::min(dw, size_t(127));

    // 12. outlierWindow ≈ filterOrder * 2 + 1, нечётное, диапазон [7, 201]
    //     Окно MAD-детектора: в 2 раза шире порядка фильтра для
    //     устойчивой локальной статистики шума
    size_t ow = p.filterOrder * 2 + 1;
    if (ow % 2 == 0) ++ow;
    p.outlierWindow = std::clamp(ow, size_t(7), size_t(201));

    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Основная точка входа
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal RobustWienerFilter::process(const Signal& input)
{
    const size_t N = input.size();
    if (N == 0)
        return Signal();

    // ── Улучшение 2: предварительная очистка от импульсных выбросов ──────────
    // OutlierDetection с MAD-детектором и медианной интерполяцией удаляет
    // одиночные и кластерные импульсы до того, как они попадут в матрицу R и
    // вектор p. Это предотвращает «загрязнение» весов w_opt статистикой выбросов.
    Signal xc = removeImpulses(input);

    // ── Улучшение 1: медианная оценка желаемого сигнала d[n] ─────────────────
    // Скользящая медиана устойчива к импульсным выбросам в отличие от
    // скользящего среднего, используемого в классическом WienerFilter.
    Signal d = estimateDesiredMedian(xc);

    // ── Построение матрицы R и вектора p на очищенном сигнале ────────────────
    ublas::matrix<double> R = buildCorrelationMatrix(xc);
    ublas::vector<double> p = buildCrossCorrelationVector(xc, d);

    // ── Тихоновская регуляризация ─────────────────────────────────────────────
    for (size_t i = 0; i < filterOrder_; ++i)
        R(i, i) += regularization_;

    // ── Решаем R · w = p ──────────────────────────────────────────────────────
    weights_ = solveLinearSystem(R, p);

    // ── Улучшение 3: применяем фильтр к ИСХОДНОМУ сигналу с zero-padding ─────
    // Линейный фильтр y[n] = wᵀ·x[n] применяется к оригинальному входу,
    // а не к предварительно очищенному — это позволяет дополнительно
    // сглаживать остаточный гауссов шум.
    // Граничное условие: при n < i используется 0.0 (нулевое дополнение),
    // а не input[0], как в классической реализации (артефакт).
    Signal output(N, 0.0);
    for (size_t n = 0; n < N; ++n) {
        double y = 0.0;
        for (size_t i = 0; i < filterOrder_; ++i) {
            // ── Улучшение 3: zero-padding вместо прижимания к x[0] ───────────
            const double xni = (n >= i) ? xc[n - i] : 0.0;
            y += weights_(i) * xni;
        }
        output[n] = y;
    }

    return output;
}

// ─────────────────────────────────────────────────────────────────────────────
// Улучшение 2: предварительная очистка выбросов
//   Использует OutlierDetection (MAD_BASED + MEDIAN_BASED)
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal RobustWienerFilter::removeImpulses(const Signal& x) const
{
    OutlierDetection detector(
        OutlierDetection::DetectionMethod::MAD_BASED,
        OutlierDetection::InterpolationMethod::MEDIAN_BASED,
        outlierThreshold_,
        outlierWindow_
    );
    return detector.process(x);
}

// ─────────────────────────────────────────────────────────────────────────────
// Улучшение 1: оценка d[n] через скользящую медиану
//   Медиана в окне desiredWindow_ устойчива к выбросам амплитуды >> σ,
//   тогда как скользящее среднее размазывает импульс по окну.
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal RobustWienerFilter::estimateDesiredMedian(const Signal& x) const
{
    const size_t N    = x.size();
    const size_t half = desiredWindow_ / 2;
    Signal d(N, 0.0);

    for (size_t n = 0; n < N; ++n) {
        const size_t lo = (n >= half) ? (n - half) : 0;
        const size_t hi = std::min(n + half, N - 1);

        // Извлекаем окно и вычисляем медиану
        std::vector<double> win(x.begin() + static_cast<std::ptrdiff_t>(lo),
                                x.begin() + static_cast<std::ptrdiff_t>(hi) + 1);
        d[n] = median(win);
    }

    return d;
}

// ─────────────────────────────────────────────────────────────────────────────
// Построение матрицы R (автокорреляция очищенного сигнала)
//   R[i,j] = (1/K) * Σ_{n=M-1}^{N-1} xc[n-i] * xc[n-j]
// ─────────────────────────────────────────────────────────────────────────────

ublas::matrix<double>
RobustWienerFilter::buildCorrelationMatrix(const Signal& xc) const
{
    const size_t N = xc.size();
    const size_t M = filterOrder_;

    ublas::matrix<double> R(M, M, 0.0);

    const size_t start = (N > M) ? (M - 1) : 0;
    const size_t K     = (N > start) ? (N - start) : 1;

    for (size_t n = start; n < N; ++n) {
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = 0; j < M; ++j) {
                R(i, j) += xc[n - i] * xc[n - j];
            }
        }
    }

    for (size_t i = 0; i < M; ++i)
        for (size_t j = 0; j < M; ++j)
            R(i, j) /= static_cast<double>(K);

    return R;
}

// ─────────────────────────────────────────────────────────────────────────────
// Построение вектора p (взаимная корреляция)
//   p[i] = (1/K) * Σ_{n=M-1}^{N-1} d[n] * xc[n-i]
// ─────────────────────────────────────────────────────────────────────────────

ublas::vector<double>
RobustWienerFilter::buildCrossCorrelationVector(const Signal& xc,
                                                 const Signal& d) const
{
    const size_t N = xc.size();
    const size_t M = filterOrder_;

    ublas::vector<double> p(M, 0.0);

    const size_t start = (N > M) ? (M - 1) : 0;
    const size_t K     = (N > start) ? (N - start) : 1;

    for (size_t n = start; n < N; ++n) {
        for (size_t i = 0; i < M; ++i) {
            p(i) += d[n] * xc[n - i];
        }
    }

    for (size_t i = 0; i < M; ++i)
        p(i) /= static_cast<double>(K);

    return p;
}
