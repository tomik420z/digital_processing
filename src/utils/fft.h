#ifndef FFT_H
#define FFT_H

/**
 * Рекурсивный алгоритм Кули-Тьюки (Cooley-Tukey FFT) — decimation-in-time.
 *
 * Требования:
 *   - Размер N должен быть степенью двойки.
 *   - Если N не является степенью двойки, вектор автоматически дополняется нулями
 *     до ближайшей сверху степени двойки (zero-padding).
 *
 * Сложность: O(N · log₂N) по времени, O(N) дополнительной памяти.
 *
 * Использует только стандартную библиотеку C++ (<complex>, <vector>, <cmath>).
 */

#include <complex>
#include <vector>
#include <cmath>
#include <stdexcept>

using Complex = std::complex<double>;
using CVector = std::vector<Complex>;

namespace fft_impl {

/// Является ли N степенью двойки
inline bool isPow2(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/// Ближайшая сверху степень двойки
inline size_t nextPow2(size_t n) {
    if (n == 0) return 1;
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

/**
 * Итеративный FFT Кули-Тьюки (in-place, decimation-in-time).
 * @param a  Вектор комплексных чисел длиной N = 2^k (модифицируется на месте).
 * @param inv false → прямое преобразование, true → обратное (IFFT, нормировка 1/N).
 */
inline void fft_inplace(CVector& a, bool inv = false)
{
    const size_t n = a.size();
    if (!isPow2(n))
        throw std::invalid_argument("fft_inplace: size must be power of 2");

    // ── Bit-reversal permutation ─────────────────────────────────────────────
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    // ── Бабочки Кули-Тьюки ──────────────────────────────────────────────────
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2.0 * M_PI / static_cast<double>(len) * (inv ? 1.0 : -1.0);
        const Complex wlen(std::cos(ang), std::sin(ang));

        for (size_t i = 0; i < n; i += len) {
            Complex w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                Complex u = a[i + j];
                Complex v = a[i + j + len / 2] * w;
                a[i + j]           = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    // ── Нормировка для IFFT ──────────────────────────────────────────────────
    if (inv) {
        const double scale = 1.0 / static_cast<double>(n);
        for (auto& c : a) c *= scale;
    }
}

} // namespace fft_impl

/**
 * Прямое DFT (FFT): вещественный вектор → комплексный спектр.
 * Размер входного вектора автоматически дополняется до степени двойки.
 *
 * @param x   Входной вещественный сигнал
 * @return    Комплексный спектр длиной N (степень двойки ≥ x.size())
 */
inline CVector fft(const std::vector<double>& x)
{
    const size_t N = fft_impl::nextPow2(x.size());
    CVector a(N, Complex(0.0, 0.0));
    for (size_t i = 0; i < x.size(); ++i)
        a[i] = Complex(x[i], 0.0);
    fft_impl::fft_inplace(a, false);
    return a;
}

/**
 * Прямое DFT (FFT): комплексный вектор → комплексный спектр.
 * Размер дополняется до степени двойки при необходимости.
 */
inline CVector fft(CVector a)
{
    const size_t N = fft_impl::nextPow2(a.size());
    a.resize(N, Complex(0.0, 0.0));
    fft_impl::fft_inplace(a, false);
    return a;
}

/**
 * Обратное DFT (IFFT): комплексный спектр → комплексный сигнал.
 * @param A   Комплексный спектр длиной N (степень двойки)
 * @return    Вещественная часть восстановленного сигнала (длина N)
 */
inline std::vector<double> ifft_real(CVector A)
{
    const size_t N = fft_impl::nextPow2(A.size());
    A.resize(N, Complex(0.0, 0.0));
    fft_impl::fft_inplace(A, true); // обратное
    std::vector<double> result(N);
    for (size_t i = 0; i < N; ++i)
        result[i] = A[i].real();
    return result;
}

#endif // FFT_H
