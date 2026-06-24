#ifndef ORDER
#error "ORDER not defined"
#else

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>

#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>

namespace {

using std::array;

constexpr int null   = 0;
constexpr int order  = ORDER;
constexpr int order2 = order * order;
constexpr int order4 = order2 * order2;

/// Sparse set implementation for a fixed, compile-time bounded integer
/// domain.
///
/// Allows constant-time insertion, removal, and lookup.
template <int MinItem, int MaxItem>
class sparse_set {
  public:
    static constexpr auto min_item = MinItem;
    static constexpr auto max_item = MaxItem;
    static constexpr auto capacity = MaxItem - MinItem + 1;

    sparse_set()                                       = default; // NOLINT
    sparse_set(const sparse_set &)                     = default;
    sparse_set(sparse_set &&)                          = default;
    auto operator=(const sparse_set &) -> sparse_set & = default;
    auto operator=(sparse_set &&) -> sparse_set &      = default;
    ~sparse_set()                                      = default;

    [[nodiscard]]
    auto size() const -> int {
        return m_size;
    }

    /// Returns index of `item` or `size()` if `item` is not in the set.
    [[nodiscard]]
    auto find(int item) const -> int {
        int index = m_sparse[item - min_item];

        if (index < 0 or index >= m_size or m_dense[index] != item) {
            return m_size;
        }

        return index;
    }

    [[nodiscard]]
    auto contains(int item) const -> bool {
        return find(item) != size();
    }

    [[nodiscard]]
    auto operator[](int index) const -> int {
        return m_dense[index];
    }

    auto insert(int item) -> void {
        if (size() != find(item)) {
            return;
        }

#ifndef NOTHROW
        if (size() == capacity) {
            throw std::length_error("how did we get here?");
        }
#endif

        int index = m_size;

        m_sparse[item - min_item] = index;
        m_dense[index]            = item;
        m_size                    = m_size + 1;
    }

    auto erase(int item) -> void {
        auto target_index = find(item);

        if (size() == target_index) {
            return;
        }

#ifndef NOTHROW
        if (size() == 0) {
            throw std::length_error("how did we get here?");
        }
#endif

        int last_index = m_size - 1;
        int last_item  = m_dense[last_index];

        m_dense[target_index]          = last_item;
        m_sparse[last_item - min_item] = target_index;
        m_size                         = m_size - 1;
    }

  private:
    /// Number of active elements in the dense array.
    int m_size = 0;

    /// Sparse lookup table.
    ///
    /// Maps normalized item value to index in m_dense
    array<int, capacity> m_sparse;

    /// Dense storage of active elements.
    array<int, capacity> m_dense;
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
    sparse_set<10, 15> s;

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
