#include "linear_system_solver.h"

namespace ublas = boost::numeric::ublas;
// ─────────────────────────────────────────────────────────────────────────────
// LU-разложение: решаем A·w = b
// ─────────────────────────────────────────────────────────────────────────────
ublas::vector<double> solveLinearSystem(
    ublas::matrix<double>  A,
    ublas::vector<double>  b
) {
    using namespace boost::numeric::ublas;

    const size_t M = A.size1();
    ublas::vector<double> w(M, 0.0);

    // Вектор перестановок для LU
    ublas::permutation_matrix<size_t> pm(M);

    // lu_factorize изменяет A на месте; возвращает 0 при успехе
    if (lu_factorize(A, pm) != 0) {
        // Вырожденная матрица — возвращаем нули
        return w;
    }

    // Решаем A·w = b; результат записывается в b
    w = b;
    lu_substitute(A, pm, w);

    return w;
}