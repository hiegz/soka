#ifndef ORDER
#error "ORDER not defined"
#else

#include <array>
#include <concepts>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <type_traits>

#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>

namespace {

using std::array;

constexpr auto null   = 0;
constexpr auto order  = ORDER;
constexpr auto order2 = order * order;
constexpr auto order4 = order2 * order2;

template <std::integral auto Min, std::integral auto Max>
class integer {
    static_assert(Min <= Max);

  public:
    using type = std::common_type_t<decltype(Min), decltype(Max)>;

    static constexpr auto min         = Min;
    static constexpr auto max         = Max;
    static constexpr auto cardinality = max - min + 1;

    [[nodiscard]]
    static constexpr auto contains(type value) -> bool {
        return value >= min and value <= max;
    }

    // NOLINTBEGIN

    constexpr integer()                          = default;
    integer(const integer &)                     = default;
    integer(integer &&)                          = default;
    auto operator=(const integer &) -> integer & = default;
    auto operator=(integer &&) -> integer &      = default;
    ~integer()                                   = default;

    constexpr integer(type value) : m_value(value) {
#ifndef NOTHROW
        if (not integer::contains(value)) {
            throw std::runtime_error("out of range");
        }
#endif
    }

    constexpr operator type() const {
        return m_value;
    }

    constexpr auto normalize() const -> integer<null, cardinality - 1> {
        return m_value - integer::min;
    }

    // NOLINTEND

  private:
    type m_value;
};

/// Index of a cell in a grid.
using index_type = integer<null, order4 - 1>;

/// Index of a subgrid or index within a subgrid depending on the context.
using subindex_type = integer<null, order2 - 1>;

/// Sparse set implementation for a fixed, compile-time bounded integer
/// domain.
///
/// Allows constant-time insertion, removal, and lookup.
template <typename T>
class sparse_set {
  public:
    // clang-format off
    using size_type = integer<null, T::cardinality>;
    using item_type = integer<T::min, T::max>;
    // clang-format on

    static constexpr auto min_value = T::min;
    static constexpr auto max_value = T::max;
    static constexpr auto capacity  = T::cardinality;

    sparse_set()                                       = default; // NOLINT
    sparse_set(const sparse_set &)                     = default;
    sparse_set(sparse_set &&)                          = default;
    auto operator=(const sparse_set &) -> sparse_set & = default;
    auto operator=(sparse_set &&) -> sparse_set &      = default;
    ~sparse_set()                                      = default;

    [[nodiscard]]
    auto size() const -> size_type {
        return m_size;
    }

    /// Returns index of `item` or `size()` if `item` is not in the set.
    [[nodiscard]]
    auto find(item_type item) const -> size_type {
        size_type index = m_sparse[item.normalize()];

        if (index < 0 or index >= m_size or m_dense[index] != item) {
            return m_size;
        }

        return index;
    }

    [[nodiscard]]
    auto contains(item_type item) const -> bool {
        return find(item) != size();
    }

    [[nodiscard]]
    auto operator[](size_type index) const -> item_type {
#ifndef NOTHROW
        if (index >= m_size) {
            throw std::out_of_range("index out of range");
        }
#endif

        return m_dense[index];
    }

    auto insert(item_type item) -> void {
        if (size() != find(item)) {
            return;
        }

#ifndef NOTHROW
        if (size() == capacity) {
            throw std::length_error("how did we get here?");
        }
#endif

        size_type index = m_size;

        m_sparse[item.normalize()] = index;
        m_dense[index]             = item;
        m_size                     = m_size + 1;
    }

    auto erase(item_type item) -> void {
        auto target_index = find(item);

        if (size() == target_index) {
            return;
        }

#ifndef NOTHROW
        if (size() == 0) {
            throw std::length_error("how did we get here?");
        }
#endif

        auto last_index = m_size - 1;
        auto last_item  = m_dense[last_index];

        m_dense[target_index]           = last_item;
        m_sparse[last_item.normalize()] = target_index;
        m_size                          = m_size - 1;
    }

  private:
    /// Number of active elements in the dense array.
    size_type m_size = 0;

    /// Sparse lookup table.
    ///
    /// Maps normalized item value to index in m_dense
    array<size_type, capacity> m_sparse;

    /// Dense storage of active elements.
    array<item_type, capacity> m_dense;
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
    sparse_set<integer<10, 15>> s;

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

    for (auto v = 10; v <= 15; ++v) {
        CHECK(s.find(v) != s.size());
    }

    // remove everything
    for (auto v = 10; v <= 15; ++v) {
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
