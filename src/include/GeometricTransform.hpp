#ifndef VD_GEO_TRAN_
#define VD_GEO_TRAN_

#include <cmath>
#include <utility>

#include "TransformationMatrix.hpp"
#include "Vec.hpp"
#include "util.hpp"

template <unsigned n>
class GeometricTransform {
   public:
    using mat_type = TransformationMatrix<n>;
    using val_type = typename mat_type::val_type;

    GeometricTransform() : _mat{} { _mat(_dim, _dim) = 1.; }
    GeometricTransform(const mat_type& mat) : _mat{mat} {
        _mat(_dim, _dim) = 1.;
    }
    GeometricTransform(mat_type&& mat) : _mat{std::move(mat)} {
        _mat(_dim, _dim) = 1.;
    }

    val_type& operator()(unsigned row, unsigned col) { return _mat(row, col); }
    val_type operator()(unsigned row, unsigned col) const {
        return _mat(row, col);
    }

    Vec<n, val_type> operator()(Vec<n, val_type> pt) {
        pt.coords = _mat * pt.coords;
        return pt;
    }

    GeometricTransform operator-() {
        // TODO
    }

    template <typename... Ts>
    friend GeometricTransform composite(Ts&&... ts) {
        return GeometricTransform{(... * ts._mat)};
    }

   private:
    const static unsigned _dim = n;
    mat_type _mat;
};

template <typename Matrix,
          std::size_t... RowInd,
          std::size_t... ColInd,
          typename... Vals>
void matrix_fill(Matrix& mat,
                 std::index_sequence<RowInd...>,
                 std::index_sequence<ColInd...>,
                 Vals... vals) {
    ((mat(RowInd, ColInd) = vals), ...);
}

// No ambiguity in this overload? Maybe due to SFINAE.
template <typename Matrix,
          typename T,
          std::size_t... RowInd,
          std::size_t... ColInd>
void matrix_fill(Matrix& mat,
                 std::index_sequence<RowInd...>,
                 std::index_sequence<ColInd...>,
                 T val) {
    ((mat(RowInd, ColInd) = static_cast<typename Matrix::val_type>(val)), ...);
}

template <typename... Args, typename Indices = std::index_sequence_for<Args...>>
auto scalingTransform(Args... args) {
    GeometricTransform<sizeof...(Args)> t{};
    matrix_fill(t, Indices{}, Indices{}, args...);
    return t;
}

template <typename... Args>
    requires(... && std::is_arithmetic_v<Args>)
auto translationTransform(Args... args) {
    GeometricTransform<sizeof...(Args)> t{};
    using Indices = std::index_sequence_for<Args...>;
    using ConstIndices =
        util::const_index_sequence<sizeof...(Args), sizeof...(Args)>;
    matrix_fill(t, Indices{}, Indices{}, 1.);
    matrix_fill(t, Indices{}, ConstIndices{}, args...);
    return t;
}

template <util::ArrayLike T>
auto translationTransform(const T& array) {
    constexpr std::size_t N = T::size();

    GeometricTransform<N> t{};
    using Indices = std::make_index_sequence<N>;
    using ConstIndices = util::const_index_sequence<N, N>;
    matrix_fill(t, Indices{}, Indices{}, 1.);
    ([&]<std::size_t... indices>(std::index_sequence<indices...>) {
        matrix_fill(t, Indices{}, ConstIndices{}, array[indices]...);
    })(std::make_index_sequence<N>{});
    return t;
}

template <unsigned axis>
auto basicRotationTransform3D_(float t) {
    GeometricTransform<3> rt{};
    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
            rt(i, j) = util::kroneckerDelta(i, j) *
                           ((1 - util::kroneckerDelta(axis, i)) * std::cos(t) +
                            util::kroneckerDelta(axis, i)) -
                       util::leviCivita(axis, i, j) * std::sin(t);
        }
    }
    return rt;
}

/**
 * @brief Rotation transform around given axis
 *
 * @param t rotation angle
 * @param axis rotation axis
 * @return GeometricTransform<3>
 */
template <typename = void>
auto rotationTransform(float t, Vec<3, float> axis) {
    const float yaw = std::atan2(axis[1], axis[0]);
    const float pitch = std::atan2(std::hypot(axis[0], axis[1]), axis[2]);

    return composite(
        basicRotationTransform3D_<2>(yaw), basicRotationTransform3D_<1>(pitch),
        basicRotationTransform3D_<2>(t), basicRotationTransform3D_<1>(-pitch),
        basicRotationTransform3D_<2>(-yaw));
}

/**
 * @brief Rotation transform by Euler angles
 *
 * @param yaw yaw angle
 * @param pitch pitch angle
 * @param rolling rolling angle
 * @return GeometricTransform<3>
 */
template <typename = void>
auto rotationTransform(float yaw, float pitch, float rolling) {
    return composite(basicRotationTransform3D_<2>(yaw),
                     basicRotationTransform3D_<1>(pitch),
                     basicRotationTransform3D_<2>(rolling));
}

template <typename = void>
auto rotationTransform(float t) {
    GeometricTransform<2> rt{};
    rt(0, 0) = std::cos(t);
    rt(0, 1) = -std::sin(t);
    rt(1, 0) = std::sin(t);
    rt(1, 1) = std::cos(t);
    return rt;
}

#endif  // VD_GEO_TRAN_
