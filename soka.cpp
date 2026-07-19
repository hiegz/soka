#ifndef ORDER
#error "ORDER not defined"
#else

#include <fstream>
#include <iostream>

#include <array>
#include <expected>
#include <filesystem>
#include <format>
#include <random>
#include <string>
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
        return size_;
    }

    /// Returns index of `item` or `size()` if `item` is not in the set.
    [[nodiscard]]
    auto find(int item) const -> int {
        int index = sparse[item - min_item];

        if (index < 0 or index >= size_ or dense[index] != item) {
            return size_;
        }

        return index;
    }

    [[nodiscard]]
    auto contains(int item) const -> bool {
        return find(item) != size();
    }

    [[nodiscard]]
    auto operator[](int index) const -> int {
        return dense[index];
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

        int index = size_;

        sparse[item - min_item] = index;
        dense[index]            = item;
        size_                   = size_ + 1;
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

        int last_index = size_ - 1;
        int last_item  = dense[last_index];

        dense[target_index]          = last_item;
        sparse[last_item - min_item] = target_index;
        size_                        = size_ - 1;
    }

  private:
    /// Number of active elements in the dense array.
    int size_ = 0;

    /// Sparse lookup table.
    ///
    /// Maps normalized item value to index in m_dense
    array<int, capacity> sparse;

    /// Dense storage of active elements.
    array<int, capacity> dense;
};

// NOLINTBEGIN

class coordinate {
  public:
    coordinate() = default;

    [[nodiscard]]
    static auto from_global(int i) -> coordinate;

    [[nodiscard]]
    static auto from_local(int si, int li) -> coordinate;

    int gi; //< cell's global index within the assignment array.
    int gx; //< cell's global x coordinate
    int gy; //< cell's global y coordinate

    int si; //< subgrid's index
    int sx; //< subgrid's x coordinate
    int sy; //< subgrid's y coordinate

    int li; //< cell's local index (within a subgrid)
    int lx; //< cell's local x coordinate (within a subgrid)
    int ly; //< cell's local y coordinate (within a subgrid)
};

auto coordinate::from_global(int i) -> coordinate {
    coordinate c;

    c.gi = i;

    c.si = c.gi / order2;
    c.sx = c.si % order;
    c.sy = c.si / order;

    c.li = i % order2;
    c.lx = c.li % order;
    c.ly = c.li / order;

    c.gx = (c.sx * order) + c.lx;
    c.gy = (c.sy * order) + c.ly;

    return c;
}

auto coordinate::from_local(int si, int li) -> coordinate {
    coordinate c;

    c.si = si;
    c.sx = c.si % order;
    c.sy = c.si / order;

    c.li = li;
    c.lx = c.li % order;
    c.ly = c.li / order;

    c.gx = (c.sx * order) + c.lx;
    c.gy = (c.sy * order) + c.ly;
    c.gi = (c.si * order2) + c.li;

    return c;
}

// NOLINTEND

class state {
    friend class swap;

  private:
    state();

  public:
    /// Reads and parses a Sudoku puzzle from the file at `path`.
    ///
    /// The file must consist of a single line of space-separated tokens.
    ///
    /// Each token is either:
    /// - `.` to represent an empty cell, or
    /// - a digit to represent a clue.
    ///
    /// Returns a candidate solution state on success, or an error message if
    /// the file cannot be read or does not conform to the expected format.
    [[nodiscard]]
    static auto load(const filesystem::path &path) -> expected<state, string>;

    /// Writes the candidate solution state to the file at `path`.
    ///
    /// Serialized as a single line of space-separated digits,
    /// representing the full candidate configuration.
    ///
    /// Returns `void` on success, or an error message if the file cannot be
    /// written.
    [[nodiscard]]
    auto save(const filesystem::path &path) const -> expected<void, string>;

    //

    /// Returns current number of conflicts.
    ///
    /// A conflict occurs whenever a digit stops appearing within a row or
    /// column. Subgrid conflicts do not contribute to this count since subgrid
    /// validity is maintained throughout state's lifetime.
    [[nodiscard]]
    auto energy() const -> int {
        return conflicts;
    }

  private:
    /// Total number of constraint violations.
    ///
    /// A conflict occurs whenever a digit stops appearing within a row or
    /// column. Subgrid conflicts do not contribute to this count since subgrid
    /// validity is maintained throughout the lifetime of this class.
    int conflicts = 0;

    /// Represents current digit assignment for every cell in the grid.
    std::array<int, order4> assignments;

    /// Assigns a new value to a cell and adjust the state accordingly.
    void assign(const coordinate &c, int value);

    /// Per-row digit frequencies.
    ///
    /// Stores the number of occurrences of a digit within a row or column.
    class frequency_map {
      private:
        array<int, (order2 * order2) + (order2 * order2)> data = {};

      public:
        [[nodiscard]]
        constexpr auto operator[](const tuple<axis, int, int> &tuple) -> int & {
            auto axis  = get<0>(tuple);
            auto coord = get<1>(tuple);
            auto digit = get<2>(tuple);

            auto axis_offset  = static_cast<int>(axis) * order4;
            auto coord_offset = order2 * coord;
            auto index        = axis_offset + coord_offset + digit;

            return data[index];
        }
    } frequencies;

    /// Open cells grouped by subgrid.
    ///
    /// Each entry contains the indices of cells that are not fixed by the
    /// puzzle clues and may therefore be modified. Grouped by subgrid to allow
    /// swaps to be performed efficiently while preserving subgrid validity.
    array<sparse_set<0, order2 - 1>, order2> open_cells;
};

// clang-format off
// NOLINTBEGIN

state::state() {
    for (int i = 0; i < order4; ++i) {
        bool duplicate;
        int  x_freq;
        int  y_freq;
        auto c = coordinate::from_global(i);

        assignments[c.gi] = c.li;
        open_cells[c.si].insert(c.li);

        x_freq    = ++frequencies[{axis::x, c.gx, c.li}];
        duplicate = x_freq > 1;

        if (duplicate) {
            conflicts++;
        }

        y_freq    = ++frequencies[{axis::y, c.gy, c.li}];
        duplicate = y_freq > 1;

        if (duplicate) {
            conflicts++;
        }
    }
}

// NOLINTEND

auto state::load(const filesystem::path &path) -> expected<state, string> {
    std::ifstream input(path);

    if (!input) {
        return unexpected(format("could not open {}", path.string()));
    }

    int   subgrid = -1;
    int   index   = -1;

    int   iod; // NOLINT
    int   di;  // NOLINT
    char  ch;  // NOLINT
    state s;

    while (input.get(ch)) {
        if (ch == ' ' or ch == '\n') {
            continue;
        }

        index++;

        if (index >= order4) {
            return unexpected("unexpected file size");
        }

        if (index % order2 == 0) {
            subgrid++;
        }

        if (ch == '0') {
            continue;
        }

        if (ch >= '1' and ch <= '9') {
            di = ch - '1';
        }
        else if (ch >= 'a' and ch <= 'z') {
            di = ch - 'a';
        }
        else if (ch >= 'A' and ch <= 'Z') {
            di = ch - 'A';
        }
        else {
            return unexpected(format("unexpected token: ", ch));
        }

        if (di < 0 or di >= order2) {
            return unexpected(format("unexpected token: {}", ch));
        }

        iod  = di;
        iod += (subgrid * order2);

        while (s.assignments[iod] != di) {
            iod  = s.assignments[iod];
            iod += (subgrid * order2);
        }

        if (not s.open_cells[subgrid].contains(iod % order2)) {
            return unexpected("unexpected duplicate: " + to_string(ch));
        }

        s.assign(coordinate::from_global(iod),   s.assignments[index]);
        s.assign(coordinate::from_global(index), di);

        s.open_cells[subgrid].erase(index % order2);
    }

    if (index + 1 != order4) {
        return unexpected("unexpected file size");
    }

    return s;
}

auto state::save(const filesystem::path &path) const -> expected<void, string> {
    std::ofstream output(path);

    if (!output) {
        return unexpected("could not open " + path.string());
    }

    char ch; // NOLINT
    int  di; // NOLINT

    for (int i = 0; i < order4; ++i) {
        di = assignments[i];

        if (di < 9) {
            ch = static_cast<char>('1' + di);
        }
        else {
            ch = static_cast<char>('a' + di);
        }

        output << ch;

        if (i + 1 < order4) {
            output << " ";
        }
    }

    return {};
}

void state::assign(const coordinate &c, int value) {
    int prev    = assignments[c.gi];
    int next    = value;

    if (prev != 0) {
        bool last;   // NOLINT
        int  x_freq; // NOLINT
        int  y_freq; // NOLINT

        x_freq = --frequencies[{axis::x, c.gx, prev}];
        last   = x_freq == 0;

        if (last) {
            conflicts++;
        }

        y_freq = --frequencies[{axis::y, c.gy, prev}];
        last   = y_freq == 0;

        if (last) {
            conflicts++;
        }
    }

    if (next != 0) {
        bool first;  // NOLINT
        int  x_freq; // NOLINT
        int  y_freq; // NOLINT

        x_freq = ++frequencies[{axis::x, c.gx, next}];
        first  = x_freq == 1;

        if (first) {
            conflicts--;
        }

        y_freq = ++frequencies[{axis::y, c.gy, next}];
        first  = y_freq == 1;

        if (first) {
            conflicts--;
        }
    }

    assignments[c.gi] = next;
}

// clang-format on

class swap {
  public:
    swap() = default;
    swap(int subgrid, int first, int second)
        : subgrid(subgrid), first(first), second(second) {}

    /// Creates a random valid swap for the given Sudoku state.
    [[nodiscard]]
    static auto random(const state &x) -> swap;

    /// Applies this swap to a given Sudoku state.
    auto apply(state &x) const;

    int subgrid;
    int first;
    int second;
};

[[nodiscard]]
auto swap::random(const state &x) -> swap {
    using distribution = uniform_int_distribution<int>;

    int subgrid = distribution(0, order2 - 1)(rng());
    int open    = x.open_cells[subgrid].size();
    int a       = distribution(0, open - 1)(rng());
    int b       = distribution(0, open - 2)(rng());

    if (b >= a) {
        b++;
    }

    return {subgrid, a, b};
}

auto swap::apply(state &x) const {
#ifndef NOTHROW
    if (not x.open_cells[subgrid].contains(first) or
        not x.open_cells[subgrid].contains(second)) {
        throw std::invalid_argument("one of swap elements are closed (fixed)");
    }
#endif

    auto c1 = coordinate::from_local(subgrid, first);
    auto c2 = coordinate::from_local(subgrid, second);

    int tmp = x.assignments[c1.gi];
    x.assign(c1, x.assignments[c2.gi]);
    x.assign(c2, tmp);
}

} // namespace

auto main() -> int {
    CPPTRACE_TRY {
        auto load_result = state::load("puzzle");

        if (not load_result) {
            cerr << load_result.error() << "\n";
            return 1;
        }

        auto s = load_result.value();

        auto save_result = s.save("state");

        if (not save_result) {
            cerr << save_result.error() << "\n";
            return 1;
        }
    }
    CPPTRACE_CATCH(const exception &e) {
        cerr << "Exception: " << e.what() << "\n";
        cerr << cpptrace::from_current_exception();
    }

    return 0;
}

#endif
