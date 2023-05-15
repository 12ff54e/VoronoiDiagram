#include "../src/Vec.hpp"
#include <iomanip>
#include "Assertion.hpp"

int main() {
    Assertion assertion;

    constexpr auto v2_origin = Vec<2>::zero();
    static_assert(v2_origin.x() == 0);

    constexpr Vec<2> v2_a{1, 2};
    static_assert(v2_a.y() == 2);

    Vec<3, int> v3_a{2, 2, 2};
    Vec<3> v3_b(v3_a);

    auto v3_c = v3_b;
    v3_b.z() += 2;
    v3_b += v3_c;

    assertion(v3_b.x() == 4 && v3_b.y() == 4 && v3_b.z() == 6,
              "Compound assignment by sum failed.");

    auto v2_b = v2_a * 2;
    auto v2_c = 10 * v2_a;
    assertion(
        v2_b.x() == 2 && v2_b.y() == 4 && v2_c.x() == 10 && v2_c.y() == 20,
        "Scalar product failed.");

    auto v2_sum = v2_b + v2_c;
    auto v2_diff = v2_c - v2_b;
    assertion(
        v2_sum == Vec<2, double>{12, 24} && v2_diff == Vec<2, double>{8, 16},
        "Sum or Diff or Comparison failed.");

    Vec<4> v4_a;
    constexpr Vec<4> v4_b{};
    constexpr Vec<4> v4_c{1, 2, 3};

    return assertion.status();
}
