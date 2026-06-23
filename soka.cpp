#include <type_traits>
#ifndef ORDER
#error "ORDER not defined"
#else

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>

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
template <auto Left, auto Right, interval_kind IntervalKind>
    requires std::is_integral_v<decltype(Left)> and
             std::is_integral_v<decltype(Right)> and
             std::is_same_v<decltype(Left), decltype(Right)>
class interval {
  public:
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
    static constexpr auto contains(uint value) -> bool {
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
template <auto Left, auto Right, interval_kind IntervalKind>
    requires std::is_integral_v<decltype(Left)> and
             std::is_integral_v<decltype(Right)> and
             std::is_same_v<decltype(Left), decltype(Right)>
class interval<Left, Right, IntervalKind>::value {
  public:
    using interval = interval<Left, Right, IntervalKind>;
    using type     = decltype(Left);

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

template <uint Left, uint Right>
class right_open {
  public:
    static_assert(Left < Right, "right_open requires Left < Right");

    static constexpr auto left  = Left;
    static constexpr auto right = Right;
    static constexpr auto min   = Left;
    static constexpr auto max   = Right - 1;
    static constexpr auto size  = Right - Left;

    [[nodiscard]]
    static constexpr auto contains(uint value) -> bool {
        return value >= min and value <= max;
    }
};

template <typename Domain>
class sparse_set {
  public:
    using domain = Domain;

    static constexpr auto min_value = domain::min;
    static constexpr auto max_value = domain::max;
    static constexpr auto capacity  = domain::size;

    ~sparse_set()                                      = default;
    sparse_set(const sparse_set &)                     = default;
    sparse_set(sparse_set &&)                          = default;
    auto operator=(const sparse_set &) -> sparse_set & = default;
    auto operator=(sparse_set &&) -> sparse_set &      = default;

    sparse_set() = // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        default;

    [[nodiscard]]
    auto size() const -> size_t {
        return m_size;
    }

    /// Returns index of `item` or `size()` if `item` is not in the set.
    [[nodiscard]]
    auto find(uint item) const -> size_t {
#ifndef NOTHROW
        if (not domain::contains(item)) {
            throw std::out_of_range("item out of range");
        }
#endif

        size_t index = m_sparse[normalize(item)];

        if (index >= m_size or m_dense[index] != item) {
            return m_size;
        }

        return index;
    }

    [[nodiscard]]
    auto contains(uint item) const -> bool {
        return find(item) != size();
    }

    [[nodiscard]]
    auto operator[](size_t index) const -> uint {
#ifndef NOTHROW
        if (index >= m_size) {
            throw std::out_of_range("index out of range");
        }
#endif

        return m_dense[index];
    }

    auto insert(uint item) -> void {
#ifndef NOTHROW
        if (not domain::contains(item)) {
            throw std::out_of_range("item out of range");
        }
#endif

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
        m_size++;
    }

    auto erase(uint item) -> void {
#ifndef NOTHROW
        if (not domain::contains(item)) {
            throw std::out_of_range("item out of range");
        }
#endif

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
        m_size--;
    }

  private:
    [[nodiscard]]
    static constexpr auto normalize(uint item) -> uint {
        return item - min_value;
    }

    [[nodiscard]]
    static constexpr auto denormalize(uint item) -> uint {
        return item + min_value;
    }

    size_t                  m_size = 0;
    array<uint, capacity>   m_dense;
    array<size_t, capacity> m_sparse;
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
    using domain = ::right_open<10, 16>;

    ::sparse_set<domain> s;

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
    size_t old_size = s.size();
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
