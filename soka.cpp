#include <fstream>
#include <random>
#ifndef ORDER
#error "ORDER not defined"
#else

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <tuple>

#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>

namespace {

inline auto rng() -> std::mt19937 & {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

using std::array;
using std::tuple;

constexpr int order  = ORDER;
constexpr int order2 = order * order;
constexpr int order4 = order2 * order2;

enum class axis : bool {
    x,
    y,
};

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

class state {
  public:
    state() { // NOLINT(cppcoreguidelines-pro-type-member-init,
              // hicpp-member-init)
        for (int gi = 0; gi < order4; ++gi) {
            int gx = gi % order2;
            int gy = gi / order2;

            int sx = gx / order;
            int sy = gy / order;
            int si = (sy * order) + sx;

            int dx = gx % order;
            int dy = gy % order;
            int di = (dy * order) + dx;

            m_assignments[gi] = di;
            m_open_cells[si].insert(di);

            if (1 < ++m_frequencies[{axis::x, gx, di}]) {
                m_conflicts++;
            }

            if (1 < ++m_frequencies[{axis::y, gy, di}]) {
                m_conflicts++;
            }
        }
    }

    state(const state &)                     = default;
    state(state &&)                          = default;
    auto operator=(const state &) -> state & = default;
    auto operator=(state &&) -> state &      = default;
    ~state()                                 = default;

    void lock(int index) {
        int gi = index;
        int gx = gi % order2;
        int gy = gi / order2;

        int sx = gx / order;
        int sy = gy / order;
        int si = (sy * order) + sx;

        int dx = gx % order;
        int dy = gy % order;
        int di = (dy * order) + dx;

        m_open_cells[si].erase(di);
    }

    void shuffle() {
        using distribution = std::uniform_int_distribution<int>;

        int si   = distribution(0, order2 - 1)(rng());
        int n    = m_open_cells[si].size();
        int a_di = distribution(0, n - 1)(rng());
        int b_di = distribution(0, n - 2)(rng());

        if (b_di >= a_di) {
            b_di++;
        }

        m_a = pos::from_subgrid_index(si, a_di);
        m_b = pos::from_subgrid_index(si, b_di);

        revert();
    }

    void revert() {
        int tmp = m_assignments[m_a.gi];
        assign(m_a, m_assignments[m_b.gi]);
        assign(m_b, tmp);
    }

    [[nodiscard]]
    auto energy() const -> int {
        return m_conflicts;
    }

    friend auto operator<<(std::ostream &os, state const &s) -> std::ostream &;

  private:
    struct pos {
        int gi;
        int gx;
        int gy;
        int si;
        int sx;
        int sy;
        int di;
        int dx;
        int dy;

        [[nodiscard]]
        static auto from_grid_index(int gi) -> struct pos {
            struct pos p; // NOLINT

            p.gi = gi;
            p.gx = p.gi % order2;
            p.gy = p.gi / order2;

            p.sx = p.gx / order;
            p.sy = p.gy / order;
            p.si = (p.sy * order) + p.sx;

            p.dx = p.gx % order;
            p.dy = p.gy % order;
            p.di = (p.dy * order) + p.dx;

            return p;
        }

        [[nodiscard]]
        static auto from_subgrid_index(int si, int di) -> struct pos {
            struct pos p; // NOLINT

            p.si = si;
            p.di = di;
            p.sx = p.si % order;
            p.sy = p.si / order;
            p.dx = p.di % order;
            p.dy = p.di / order;
            p.gx = (p.sx * order) + p.dx;
            p.gy = (p.sy * order) + p.dy;
            p.gi = (p.gy * order2) + p.gx;

            return p;
        }
    };

    /// Total number of constraint violations.
    ///
    /// A conflict occurs whenever a digit stops appearing within a row or
    /// column. Subgrid conflicts do not contribute to this count since subgrid
    /// validity is maintained throughout the lifetime of this class.
    int m_conflicts = 0;

    /// Represents current digit assignment for every cell in the grid.
    ///
    /// Stores in row-major order. Fixed clues retain their original values,
    /// while open cells may be modified.
    array<int, order4> m_assignments;

    /// Assigns a new value to a cell and adjust the state accordingly.
    void assign(struct pos const &p, int digit) {
        int prev = m_assignments[p.gi];
        int next = digit;

        if (prev != 0) {
            if (--m_frequencies[{axis::x, p.gx, prev}] == 0) {
                m_conflicts++;
            }

            if (--m_frequencies[{axis::y, p.gy, prev}] == 0) {
                m_conflicts++;
            }
        }

        if (next != 0) {
            if (++m_frequencies[{axis::x, p.gx, next}] == 1) {
                m_conflicts--;
            }

            if (++m_frequencies[{axis::y, p.gy, next}] == 1) {
                m_conflicts--;
            }
        }

        m_assignments[p.gi] = next;
    }

    /// Per-row digit frequencies.
    ///
    /// Stores the number of occurrences of a digit within a row or column.
    class frequency_map {
      private:
        array<int, (order2 * order2) + (order2 * order2)> m_data = {};

      public:
        [[nodiscard]]
        constexpr auto operator[](const tuple<axis, int, int> &tuple) -> int & {
            auto axis  = std::get<0>(tuple);
            auto coord = std::get<1>(tuple);
            auto digit = std::get<2>(tuple);

            auto axis_offset  = static_cast<int>(axis) * order4;
            auto coord_offset = order2 * coord;
            auto index        = axis_offset + coord_offset + digit;

            return m_data[index];
        }
    } m_frequencies;

    /// Open cells grouped by subgrid.
    ///
    /// Each entry contains the indices of cells that are not fixed by the
    /// puzzle clues and may therefore be modified. Grouped by subgrid to allow
    /// swaps to be performed efficiently while preserving subgrid validity.
    array<sparse_set<0, order2 - 1>, order2> m_open_cells;

    /// Index of the most recent left hand swap operand used for reversal.
    struct pos m_a;

    /// Index of the most recent right hand swap operand used for reversal.
    struct pos m_b;
};

auto operator<<(std::ostream &os, const state &s) -> std::ostream & {
    for (int i = 0; i < order4; ++i) {
        os << s.m_assignments[i];
        if (i + 1 < order4) {
            os << ' ';
        }
    }

    return os;
}

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

        std::ofstream file("state");
        state         s;

        for (int i = 0; i < 100; ++i) {
            s.shuffle();
        }

        file << s;
    }
    CPPTRACE_CATCH(const std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
        std::cerr << cpptrace::from_current_exception();
    }

    return 0;
}

#endif
