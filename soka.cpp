#ifndef ORDER
#error "ORDER not defined"
#else

#include <cstdint>

using uint = uint32_t;

constexpr uint order  = ORDER;
constexpr uint order2 = order * order;
constexpr uint order4 = order2 * order2;

auto main() -> int {
    return 0;
}

#endif
