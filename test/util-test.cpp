#include "../src/include/util.hpp"
#include "Assertion.hpp"

int main(int argc, char const* argv[]) {
    static_assert(std::is_same_v<util::const_index_sequence<3, 42>,
                                 std::index_sequence<42u, 42u, 42u>>);
    static_assert(util::leviCivita(0, 1, 2) == 1);
    static_assert(util::leviCivita(0, 2, 1) == -1);
    static_assert(util::leviCivita(0, 1, 1) == 0);
    static_assert(util::leviCivita(0, 3, 1, 2) == 1);
    static_assert(util::kroneckerDelta(0, 0, 0, 0) == 1);
    static_assert(util::kroneckerDelta(0, 1, 0, 0) == 0);

    return 0;
}
