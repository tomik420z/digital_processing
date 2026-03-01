#ifndef LINEAR_SYSTEM_SOLVER_H
#define LINEAR_SYSTEM_SOLVER_H

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/lu.hpp>

/**
* Решить систему A·w = b методом LU-разложения (boost ublas)
* @return вектор решения w, или нулевой вектор при вырожденной матрице
*/
boost::numeric::ublas::vector<double> solveLinearSystem(
    boost::numeric::ublas::matrix<double> A,
    boost::numeric::ublas::vector<double> b
);

#endif
