#ifndef VD_VEC_
#define VD_VEC_

#include <array>
#include <cmath>     // std::sqrt
#include <concepts>  // std::convertible_to
#include <utility>   // std::index_sequence

/**
 * @brief A vector in Euclidean space.
 *
 * @tparam D Dimension of the underlying space.
 * @tparam T Type of coordinate.
 */
template <std::size_t D, typename T>
class Vec;

template <std::size_t D, typename T>
class VecBase {
    static_assert(D > 0, "D cannot be 0.");

   public:
    static constexpr std::size_t dim = D;
    using value_type = T;
    using vec_type = Vec<dim, value_type>;

   protected:
    std::array<value_type, dim> coord;

   public:
    constexpr VecBase() = default;

    template <typename... Ts>
        requires(sizeof...(Ts) <= dim &&
                 (std::convertible_to<Ts, value_type> && ...))
    constexpr VecBase(Ts... vs) noexcept
        : coord{static_cast<value_type>(vs)...} {}

    /**
     * @brief Conversion constructor from supported type
     *
     * @param other another vec
     */
    template <typename U>
        requires std::convertible_to<U, T>
    VecBase(Vec<dim, U> other) noexcept {
        ([&]<std::size_t... indices>(std::index_sequence<indices...>,
                                     Vec<dim, U> v) {
            (..., (coord[indices] = static_cast<value_type>(v[indices])));
        })(std::make_index_sequence<dim>{}, other);
    }

    // zero element

    static constexpr vec_type zero() noexcept {
        return VecBase<dim, value_type>{};
    }

    // element access

    T& operator[](std::size_t i) noexcept { return coord[i]; }

    const T& operator[](std::size_t i) const noexcept { return coord[i]; }

    // comparison operations

    friend bool operator==(const vec_type& lhs, const vec_type& rhs) {
        return ([]<std::size_t... indices>(std::index_sequence<indices...>,
                                           const VecBase& v1,
                                           const VecBase& v2) {
            return (... && (v1[indices] == v2[indices]));
        })(std::make_index_sequence<dim>{}, lhs, rhs);
    }

    friend bool operator!=(const vec_type& lhs, const vec_type& rhs) {
        return !operator==(lhs, rhs);
    }

    // arithmetic operations

#define vec_compound_assign_2_(op)                                             \
    ([]<std::size_t... indices>(std::index_sequence<indices...>, vec_type& v1, \
                                const vec_type& v2) -> vec_type& {             \
        (..., (v1[indices] op v2[indices]));                                   \
        return v1;                                                             \
    })

    friend vec_type& operator+=(vec_type& lhs, const vec_type& rhs) noexcept {
        return vec_compound_assign_2_(+=)(std::make_index_sequence<dim>{}, lhs,
                                          rhs);
    }
    friend vec_type& operator-=(vec_type& lhs, const vec_type& rhs) noexcept {
        return vec_compound_assign_2_(-=)(std::make_index_sequence<dim>{}, lhs,
                                          rhs);
    }

#define vec_compound_assign_1_(op)                                             \
    ([]<std::size_t... indices>(std::index_sequence<indices...>, vec_type& v1, \
                                const T& s) -> vec_type& {                     \
        (..., (v1[indices] op s));                                             \
        return v1;                                                             \
    })

    friend vec_type& operator*=(vec_type& lhs, const T& scalar) noexcept {
        return vec_compound_assign_1_(*=)(std::make_index_sequence<dim>{}, lhs,
                                          scalar);
    }

    friend vec_type& operator/=(vec_type& lhs, const T& scalar) noexcept {
        return vec_compound_assign_1_(/=)(std::make_index_sequence<dim>{}, lhs,
                                          scalar);
    }

    friend vec_type operator+(vec_type lhs, const vec_type& rhs) noexcept {
        return lhs += rhs;
    }
    friend vec_type operator-(vec_type lhs, const vec_type& rhs) noexcept {
        return lhs -= rhs;
    }

    friend vec_type operator*(vec_type vec, const T& scalar) noexcept {
        return vec *= scalar;
    }
    friend vec_type operator*(const T& scalar, vec_type vec) noexcept {
        return vec *= scalar;
    }

    friend vec_type operator/(vec_type vec, const T& scalar) noexcept {
        return vec /= scalar;
    }

    // conversion operator

    // convert to the underlying coordinate array
    constexpr operator std::array<value_type, dim>() const { return coord; }

    // properties

    static constexpr std::size_t size() noexcept { return dim; }

    T L2_norm_square() const {
        T norm{};
        for (auto& c : coord) { norm += c * c; }
        return norm;
    }

    T mag() const { return std::sqrt(L2_norm_square()); }
};

/**
 * @brief Generic Vector type of any dimension, default to consisting double as
 * coordinates
 *
 * @tparam D Dimension
 * @tparam T Underlying types of each dimension
 */
template <std::size_t D, typename T = double>
class Vec : public VecBase<D, T> {
    using base_type = VecBase<D, T>;

   public:
    using base_type::base_type;

    constexpr Vec(const base_type& b) : base_type(b) {}
    constexpr Vec(const base_type&& b) : base_type(std::move(b)) {}
};

template <typename T>
class Vec<2, T> : public VecBase<2, T> {
    using base_type = VecBase<2, T>;

   public:
    using base_type::base_type;
    constexpr Vec(const base_type& b) : base_type(b) {}
    constexpr Vec(const base_type&& b) : base_type(std::move(b)) {}

    T& x() noexcept { return this->coord[0]; }
    T& y() noexcept { return this->coord[1]; }

    constexpr const T& x() const noexcept { return this->coord[0]; }
    constexpr const T& y() const noexcept { return this->coord[1]; }
};

template <typename T>
class Vec<3, T> : public VecBase<3, T> {
    using base_type = VecBase<3, T>;

   public:
    using base_type::base_type;
    constexpr Vec(const base_type& b) : base_type(b) {}
    constexpr Vec(const base_type&& b) : base_type(std::move(b)) {}

    T& x() noexcept { return this->coord[0]; }
    T& y() noexcept { return this->coord[1]; }
    T& z() noexcept { return this->coord[2]; }

    const T& x() const noexcept { return this->coord[0]; }
    const T& y() const noexcept { return this->coord[1]; }
    const T& z() const noexcept { return this->coord[2]; }
};

#endif
