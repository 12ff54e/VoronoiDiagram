#ifndef VD_TRANSFORMATION_
#define VD_TRANSFORMATION_

#include <array>
#include <utility>
#include "Vec.hpp"

template <unsigned n>
class TransformationMatrix {
   public:
    using val_type = float;

   private:
    const static unsigned _dim = n + 1;
    // This array will be zero-initialized, when the class has trivial
    // implicitly-defined or defaulted default constructor.
    std::array<val_type, (n + 1) * (n + 1)> _mat_elems;

   public:
    TransformationMatrix() { _mat_elems.fill(0.); }

    inline unsigned dim() const { return _dim; }

    // element access

    val_type& operator()(unsigned row, unsigned col) {
        return _mat_elems[row * _dim + col];
    }
    val_type operator()(unsigned row, unsigned col) const {
        return _mat_elems[row * _dim + col];
    }

    Vec<n, val_type> operator*(Vec<n, val_type> coord) {
        unsigned last_row = _dim * (_dim - 1);
        for (unsigned i = 0; i < _dim - 1; ++i) {
            for (unsigned j = 0; j < _dim; ++j) {
                // buffer the product in last row of matrix, which is of no use.
                _mat_elems[last_row] += operator()(i, j) *
                                        (j < _dim - 1 ? coord[j] : 1.);
            }
            ++last_row;
        }
        last_row = _dim * (_dim - 1);
        for (unsigned i = 0; i < _dim - 1; ++i) {
            coord[i] = std::exchange(_mat_elems[last_row++], 0.);
        }
        return coord;
    }

    TransformationMatrix& operator*=(const TransformationMatrix& t) {
        _mat_elems.back() = 0.;
        for (unsigned i = 0; i < _dim - 1; ++i) {
            for (unsigned j = 0, last_row = _dim * (_dim - 1); j < _dim;
                 ++j, ++last_row) {
                for (unsigned k = 0; k < _dim; ++k) {
                    _mat_elems[last_row] += operator()(i, k) * t(k, j);
                }
            }

            for (unsigned ind = i * _dim, last_row = _dim * (_dim - 1);
                 ind < (i + 1) * _dim; ++ind, ++last_row) {
                _mat_elems[ind] = std::exchange(_mat_elems[last_row], 0.);
            }
        }
        _mat_elems.back() = 1.;
        return *this;
    }

    friend TransformationMatrix operator*(TransformationMatrix t1,
                                          const TransformationMatrix& t2) {
        t1 *= t2;
        return t1;
    }
};

#endif  // VD_TRANSFORMATION_
