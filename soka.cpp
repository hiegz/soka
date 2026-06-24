#ifndef ORDER
#error "ORDER not defined"
#else

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <type_traits>

#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>

using uint = uint32_t;

constexpr uint order  = ORDER;
constexpr uint order2 = order * order;
constexpr uint order4 = order2 * order2;

namespace {

using std::array;

constexpr auto null = 0;

///
///
///
enum class interval_kind : std::uint8_t {
    closed,
    open,
    left_open,
    right_open,
};

///
///
///
template <std::integral auto Left, std::integral auto Right,
          interval_kind IntervalKind>
class interval {
  public:
    using value_type = std::common_type_t<decltype(Left), decltype(Right)>;
    class value;

    // clang-format off

    static_assert(IntervalKind != interval_kind::closed     or Left <= Right, "closed intervals require Left <= Right");
    static_assert(IntervalKind != interval_kind::open       or Left <  Right, "open intervals require Left < Right");
    static_assert(IntervalKind != interval_kind::left_open  or Left <  Right, "left-open intervals require Left < Right");
    static_assert(IntervalKind != interval_kind::right_open or Left <  Right, "right-open intervals require Left < Right");

    static constexpr auto left_open  = (IntervalKind == interval_kind::open or IntervalKind == interval_kind::left_open);
    static constexpr auto right_open = (IntervalKind == interval_kind::open or IntervalKind == interval_kind::right_open);

    static constexpr auto left  = Left;
    static constexpr auto right = Right;
    static constexpr auto min   = (left_open  ? Left  + 1 : Left);
    static constexpr auto max   = (right_open ? Right - 1 : Right);
    static constexpr auto size  = max - min + 1;

    // clang-format on

    [[nodiscard]]
    static constexpr auto contains(value_type value) -> bool {
        return value >= min and value <= max;
    }
};

template <auto Left, auto Right>
using closed_interval = interval<Left, Right, interval_kind::closed>;

template <auto Left, auto Right>
using open_interval = interval<Left, Right, interval_kind::open>;

template <auto Left, auto Right>
using left_open_interval = interval<Left, Right, interval_kind::left_open>;

template <auto Left, auto Right>
using right_open_interval = interval<Left, Right, interval_kind::right_open>;

///
///
///
template <std::integral auto Left, std::integral auto Right,
          interval_kind IntervalKind>
class interval<Left, Right, IntervalKind>::value {
  public:
    using type = interval::value_type;

    constexpr value() = default;

    // NOLINTBEGIN

    constexpr value(type value) : m_value(value) {
#ifndef NOTHROW
        if (not interval::contains(value)) {
            throw std::runtime_error("out of range");
        }
#endif
    }

    constexpr operator type() const {
        return m_value;
    }

    // NOLINTEND

  private:
    type m_value;
};

/// Index of a cell in a grid.
using index = right_open_interval<null, order4>;

/// Index of a subgrid or index within a subgrid depending on the context.
using subindex = right_open_interval<null, order2>;

template <typename Interval>
class sparse_set {
  public:
    // clang-format off
    using index = closed_interval<null, Interval::size>;
    using item  = Interval;
    // clang-format on

    static_assert(index::size == item::size + 1);

    static constexpr auto min_value = item::min;
    static constexpr auto max_value = item::max;
    static constexpr auto capacity  = item::size;

    ~sparse_set()                                      = default;
    sparse_set(const sparse_set &)                     = default;
    sparse_set(sparse_set &&)                          = default;
    auto operator=(const sparse_set &) -> sparse_set & = default;
    auto operator=(sparse_set &&) -> sparse_set &      = default;

    sparse_set() = // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        default;

    [[nodiscard]]
    auto size() const -> index::value {
        return m_size;
    }

    /// Returns index of `item` or `size()` if `item` is not in the set.
    [[nodiscard]]
    auto find(item::value item) const -> index::value {
        auto index = m_sparse[normalize(item)];

        if (index >= m_size or m_dense[index] != item) {
            return m_size;
        }

        return index;
    }

    [[nodiscard]]
    auto contains(item::value item) const -> bool {
        return find(item) != size();
    }

    [[nodiscard]]
    auto operator[](index::value index) const -> item::value {
#ifndef NOTHROW
        if (index >= m_size) {
            throw std::out_of_range("index out of range");
        }
#endif

        return m_dense[index];
    }

    auto insert(item::value item) -> void {
        if (size() != find(item)) {
            return;
        }

#ifndef NOTHROW
        if (size() == capacity) {
            throw std::length_error("how did we get here?");
        }
#endif

        size_t index              = m_size;
        m_sparse[normalize(item)] = index;
        m_dense[index]            = item;
        m_size                    = m_size + 1;
    }

    auto erase(item::value item) -> void {
        size_t i_target = find(item);

        if (size() == i_target) {
            return;
        }

#ifndef NOTHROW
        if (size() == 0) {
            throw std::length_error("how did we get here?");
        }
#endif

        size_t i_last             = m_size - 1;
        uint   last               = m_dense[i_last];
        m_dense[i_target]         = last;
        m_sparse[normalize(last)] = i_target;
        m_size                    = m_size - 1;
    }

  private:
    [[nodiscard]]
    static constexpr auto normalize(item::value item) -> index::value {
        return item - min_value;
    }

    /// ...
    index::value m_size = 0;

    /// ...
    array<typename index::value, capacity> m_sparse;

    /// ...
    array<typename item::value, capacity> m_dense;
};

#define CHECK(expr)                                                            \
    {                                                                          \
        if (!(expr)) {                                                         \
            std::cout << "CHECK FAILED: " #expr << " (line " << __LINE__       \
                      << ")\n";                                                \
            return 1;                                                          \
        }                                                                      \
    }

auto test_sparse_set() -> int {
    ::sparse_set<::right_open_interval<10, 16U>> s;

    // empty
    CHECK(s.size() == 0);
    CHECK(s.find(10) == s.size());

    // insert one
    s.insert(10);

    CHECK(s.size() == 1);
    CHECK(s.find(10) == 0);
    CHECK(s[0] == 10);

    // insert more
    s.insert(12);
    s.insert(14);

    CHECK(s.size() == 3);
    CHECK(s.find(12) != s.size());
    CHECK(s.find(14) != s.size());

    // duplicate insert
    auto old_size = s.size();
    s.insert(12);

    CHECK(s.size() == old_size);

    // erase existing
    s.erase(12);

    CHECK(s.size() == 2);
    CHECK(s.find(12) == s.size());

    // remaining elements still present
    CHECK(s.find(10) != s.size());
    CHECK(s.find(14) != s.size());

    // erase missing
    old_size = s.size();
    s.erase(13);

    CHECK(s.size() == old_size);

    // fill to capacity
    s.insert(11);
    s.insert(12);
    s.insert(13);
    s.insert(15);

    CHECK(s.size() == s.capacity);

    for (uint v = 10; v <= 15; ++v) {
        CHECK(s.find(v) != s.size());
    }

    // remove everything
    for (uint v = 10; v <= 15; ++v) {
        s.erase(v);
    }

    CHECK(s.size() == 0);

    return 0;
}

} // namespace

auto main() -> int {
    CPPTRACE_TRY {
        if (1 == ::test_sparse_set()) {
            return 1;
        }
    }
    CPPTRACE_CATCH(const std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
        std::cerr << cpptrace::from_current_exception();
    }

    return 0;
}

#endif
