#include "wiener_filter.h"
#include "utils/linear_system_solver.h"

#include <stdexcept>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Конструктор / setParameters
// ─────────────────────────────────────────────────────────────────────────────

WienerFilter::WienerFilter(size_t filterOrder,
                           size_t desiredWindow,
                           double regularization)
    : filterOrder_(filterOrder),
      desiredWindow_(desiredWindow),
      regularization_(regularization)
{
    if (filterOrder_ == 0) {
        throw std::invalid_argument("WienerFilter: filterOrder must be > 0");
    }
    if (desiredWindow_ == 0) {
        throw std::invalid_argument("WienerFilter: desiredWindow must be > 0");
    }
    if (regularization_ < 0.0) {
        throw std::invalid_argument("WienerFilter: regularization must be >= 0");
    }
    weights_.resize(filterOrder_, 0.0);
}

void WienerFilter::setParameters(size_t filterOrder,
                                  size_t desiredWindow,
                                  double regularization)
{
    if (filterOrder == 0)
        throw std::invalid_argument("WienerFilter: filterOrder must be > 0");
    if (desiredWindow == 0)
        throw std::invalid_argument("WienerFilter: desiredWindow must be > 0");
    if (regularization < 0.0)
        throw std::invalid_argument("WienerFilter: regularization must be >= 0");

    filterOrder_    = filterOrder;
    desiredWindow_  = desiredWindow;
    regularization_ = regularization;
    weights_.resize(filterOrder_, 0.0);
}

std::string WienerFilter::getName() const
{
    return "WienerFilter_ord" + std::to_string(filterOrder_) +
           "_win" + std::to_string(desiredWindow_);
}

std::vector<double> WienerFilter::getWeights() const
{
    return std::vector<double>(weights_.begin(), weights_.end());
}

// ─────────────────────────────────────────────────────────────────────────────
// Основная точка входа
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal WienerFilter::process(const Signal& input)
{
    const size_t N = input.size();
    if (N == 0)
        return Signal();

    // 1. Оцениваем желаемый сигнал d[n] (скользящее среднее)
    Signal d = estimateDesired(input);

    // 2. Строим автокорреляционную матрицу R и вектор p
    ublas::matrix<double> R = buildCorrelationMatrix(input);
    ublas::vector<double> p = buildCrossCorrelationVector(input, d);

    // 3. Добавляем тихоновскую регуляризацию к диагонали R
    for (size_t i = 0; i < filterOrder_; ++i) {
        R(i, i) += regularization_;
    }

    // 4. Решаем R · w = p
    weights_ = solveLinearSystem(R, p);

    // 5. Применяем фильтр: y[n] = wᵀ · x[n]
    Signal output(N, 0.0);
    for (size_t n = 0; n < N; ++n) {
        double y = 0.0;
        for (size_t i = 0; i < filterOrder_; ++i) {
            const size_t idx = (n >= i) ? (n - i) : 0;
            y += weights_(i) * input[idx];
        }
        output[n] = y;
    }

    return output;
}

// ─────────────────────────────────────────────────────────────────────────────
// Построение матрицы R
//   R[i,j] = (1/K) * Σ_{n=M-1}^{N-1} x[n-i] * x[n-j]
// ─────────────────────────────────────────────────────────────────────────────

ublas::matrix<double>
WienerFilter::buildCorrelationMatrix(const Signal& x) const
{
    const size_t N = x.size();
    const size_t M = filterOrder_;

    ublas::matrix<double> R(M, M, 0.0);

    // Суммируем по всем позициям, где доступны все M задержанных отсчётов
    const size_t start = (N > M) ? (M - 1) : 0;
    const size_t K     = (N > start) ? (N - start) : 1;

    for (size_t n = start; n < N; ++n) {
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = 0; j < M; ++j) {
                R(i, j) += x[n - i] * x[n - j];
            }
        }
    }

    // Нормируем
    for (size_t i = 0; i < M; ++i) {
        for (size_t j = 0; j < M; ++j) {
            R(i, j) /= static_cast<double>(K);
        }
    }

    return R;
}

// ─────────────────────────────────────────────────────────────────────────────
// Построение вектора p
//   p[i] = (1/K) * Σ_{n=M-1}^{N-1} d[n] * x[n-i]
// ─────────────────────────────────────────────────────────────────────────────

ublas::vector<double>
WienerFilter::buildCrossCorrelationVector(const Signal& x,
                                           const Signal& d) const
{
    const size_t N = x.size();
    const size_t M = filterOrder_;

    ublas::vector<double> p(M, 0.0);

    const size_t start = (N > M) ? (M - 1) : 0;
    const size_t K     = (N > start) ? (N - start) : 1;

    for (size_t n = start; n < N; ++n) {
        for (size_t i = 0; i < M; ++i) {
            p(i) += d[n] * x[n - i];
        }
    }

    for (size_t i = 0; i < M; ++i)
        p(i) /= static_cast<double>(K);

    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Оценка желаемого сигнала d[n] — скользящее среднее длиной desiredWindow_
// ─────────────────────────────────────────────────────────────────────────────

SignalProcessor::Signal WienerFilter::estimateDesired(const Signal& x) const
{
    const size_t N    = x.size();
    const size_t half = desiredWindow_ / 2;
    Signal d(N, 0.0);

    for (size_t n = 0; n < N; ++n) {
        const size_t lo  = (n >= half) ? (n - half) : 0;
        const size_t hi  = std::min(n + half, N - 1);
        double       sum = 0.0;
        for (size_t k = lo; k <= hi; ++k)
            sum += x[k];
        d[n] = sum / static_cast<double>(hi - lo + 1);
    }

    return d;
}
