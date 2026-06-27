#ifndef ORDER
#error "ORDER not defined"
#else

#include <fstream>
#include <iostream>

#include <array>
#include <random>
#include <tuple>

#include <exception>
#include <stdexcept>

#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>

using namespace std;

namespace {

inline auto rng() -> mt19937 & {
    static mt19937 gen(random_device{}());
    return gen;
}

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
            throw length_error("how did we get here?");
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
            throw length_error("how did we get here?");
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

class coord {
  public:
    coord() = default;

    coord(int subgrid, int cell) : m_subgrid(subgrid), m_cell(cell) {
#ifndef NOTHROW
        if (subgrid < 0 or subgrid >= order2) {
            throw out_of_range("square index out of range");
        }

        if (cell < 0 or cell >= order2) {
            throw out_of_range("cell index out of range");
        }
#endif
    }

    [[nodiscard]]
    auto subgrid() const -> int {
        return m_subgrid;
    }

    [[nodiscard]]
    auto cell() const -> int {
        return m_cell;
    }

    [[nodiscard]]
    auto row() const -> int {
        int sy = m_subgrid / order;
        int gy = sy * order;
        int dy = m_cell / order;

        return gy + dy;
    }

    [[nodiscard]]
    auto column() const -> int {
        int sx = m_subgrid % order;
        int gx = sx * order;
        int dx = m_cell % order;

        return gx + dx;
    }

  private:
    int m_subgrid;
    int m_cell;
};

class state {
  public:
    state() { // NOLINT(cppcoreguidelines-pro-type-member-init,
              // hicpp-member-init)
        for (int i = 0; i < order2; ++i) {
            for (int j = 0; j < order2; ++j) {
                int subgrid = i;
                int cell    = j;
                int digit   = j;

                auto c = coord(subgrid, cell);

                m_assignments[c] = digit;
                m_open_cells[subgrid].insert(cell);

                if (1 < ++m_frequencies[{axis::x, c.column(), digit}]) {
                    m_conflicts++;
                }

                if (1 < ++m_frequencies[{axis::y, c.row(), digit}]) {
                    m_conflicts++;
                }
            }
        }
    }

    void lock(const coord &c) {
        m_open_cells[c.subgrid()].erase(c.cell());
    }

    void shuffle() {
        using distribution = uniform_int_distribution<int>;

        int subgrid = distribution(0, order2 - 1)(rng());
        int open    = m_open_cells[subgrid].size();
        int cell1   = distribution(0, open - 1)(rng());
        int cell2   = distribution(0, open - 2)(rng());

        if (cell2 >= cell1) {
            cell2++;
        }

        m_a = coord(subgrid, cell1);
        m_b = coord(subgrid, cell2);

        revert();
    }

    void revert() {
        int a = m_assignments[m_a];
        int b = m_assignments[m_b];

        assign(m_a, b);
        assign(m_b, a);
    }

    [[nodiscard]]
    auto energy() const -> int {
        return m_conflicts;
    }

    friend auto operator<<(ostream &os, state const &s) -> ostream &;

  private:
    /// Total number of constraint violations.
    ///
    /// A conflict occurs whenever a digit stops appearing within a row or
    /// column. Subgrid conflicts do not contribute to this count since subgrid
    /// validity is maintained throughout the lifetime of this class.
    int m_conflicts = 0;

    /// Represents current digit assignment for every cell in the grid.
    class assignments {
      private:
        array<int, order4> m_data;

        [[nodiscard]]
        static auto to_index(const coord &c) -> int {
            return (c.subgrid() * order2) + c.cell();
        }

      public:
        [[nodiscard]]
        auto operator[](const coord &c) -> int & {
            return m_data[to_index(c)];
        }

        [[nodiscard]]
        auto operator[](const coord &c) const -> int {
            return m_data[to_index(c)];
        }
    } m_assignments;

    /// Assigns a new value to a cell and adjust the state accordingly.
    void assign(const coord &c, int digit) {
        int column = c.column();
        int row    = c.row();
        int prev   = m_assignments[c];
        int next   = digit;

        if (prev != 0) {
            if (--m_frequencies[{axis::x, column, prev}] == 0) {
                m_conflicts++;
            }

            if (--m_frequencies[{axis::y, row, prev}] == 0) {
                m_conflicts++;
            }
        }

        if (next != 0) {
            if (++m_frequencies[{axis::x, column, next}] == 1) {
                m_conflicts--;
            }

            if (++m_frequencies[{axis::y, row, next}] == 1) {
                m_conflicts--;
            }
        }

        m_assignments[c] = next;
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
            auto axis  = get<0>(tuple);
            auto coord = get<1>(tuple);
            auto digit = get<2>(tuple);

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
    coord m_a{};

    /// Index of the most recent right hand swap operand used for reversal.
    coord m_b{};
};

auto operator<<(ostream &os, const state &s) -> ostream & {
    for (int i = 0; i < order2; ++i) {
        for (int j = 0; j < order2; ++j) {
            os << s.m_assignments[coord(i, j)];
            if (i + 1 < order4) {
                os << ' ';
            }
        }
    }

    return os;
}

#define CHECK(expr)                                                            \
    {                                                                          \
        if (!(expr)) {                                                         \
            cout << "CHECK FAILED: " #expr << " (line " << __LINE__ << ")\n";  \
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

        ofstream file("state");
        state    s;

        for (int i = 0; i < 100; ++i) {
            s.shuffle();
        }

        file << s;
    }
    CPPTRACE_CATCH(const exception &e) {
        cerr << "Exception: " << e.what() << "\n";
        cerr << cpptrace::from_current_exception();
    }

    return 0;
}

#endif
