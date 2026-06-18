#ifndef ORDER
#error "ORDER not defined"
#else

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using uint = uint32_t;

constexpr uint order  = ORDER;
constexpr uint order2 = order * order;
constexpr uint order4 = order2 * order2;

namespace {

using std::array;

template <uint MinValue, uint MaxValue>
class sparse_set {
  public:
    static constexpr uint   min_value = MinValue;
    static constexpr uint   max_value = MaxValue;
    static constexpr size_t capacity  = MaxValue - MinValue + 1;

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
        if (item < min_value or item > max_value) {
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
        if (item < min_value or item > max_value) {
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
        if (item < min_value or item > max_value) {
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
    ::sparse_set<10, 15> s;

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
    if (1 == ::test_sparse_set()) {
        return 1;
    }

    return 0;
}

#endif
