#include "robust_wiener_filter.h"
#include "utils/linear_system_solver.h"
#include "utils/median.h"

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
