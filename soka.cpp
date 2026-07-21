#ifndef ORDER
#error "ORDER not defined"
#else

#include <fstream>
#include <iostream>

#include <cmath>
#include <cstddef>

#include <array>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <exception>
#include <stdexcept>

#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>

using namespace std;

namespace {

constexpr const char *INFO   = "[  info  ] ";
constexpr const char *STATUS = "[ status ] ";
constexpr const char *ERROR  = "[  fail  ] ";
constexpr const char *WHY    = "[ reason ] ";

auto usage(string_view exe) -> string {
    return format("usage: {} <puzzle-file> <solution-file>", exe);
}

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

class xycache {
  public:
    xycache();

    [[nodiscard]]
    constexpr auto x(int index) const -> int;

    [[nodiscard]]
    constexpr auto y(int index) const -> int;

  private:
    array<int, order4> xs;
    array<int, order4> ys;
};

xycache::xycache() {
    int gi;
    int si;
    int sx;
    int sy;
    int li;
    int lx;
    int ly;

    for (gi = 0; gi < order4; ++gi) {
        si = gi / order2;
        sx = si % order;
        sy = si / order;

        li = gi % order2;
        lx = li % order;
        ly = li / order;

        xs[gi] = (sx * order) + lx;
        ys[gi] = (sy * order) + ly;
    }
}

constexpr auto xycache::x(int index) const -> int {
    return xs[index];
}

constexpr auto xycache::y(int index) const -> int {
    return ys[index];
}

class global;
class local;

class global {
  public:
    global() = default;
    global(int index);    // NOLINT
    global(local c);      // NOLINT
    operator int() const; // NOLINT

    int index;

    [[nodiscard]]
    constexpr auto x() const -> int;

    [[nodiscard]]
    constexpr auto y() const -> int;
};

class local {
  public:
    local() = default;
    local(int c);    // NOLINT
    local(global c); // NOLINT
    local(int subgrid, int index);

    int subgrid;
    int index;

    [[nodiscard]]
    constexpr auto x() const -> int;

    [[nodiscard]]
    constexpr auto y() const -> int;
};

// clang-format off

global::global(int index)
    : index(index)
{}

global::global(local c)
    : global((c.subgrid * order2) + c.index)
{}

global::operator int() const {
    return index;
}

constexpr auto global::x() const -> int {
    static xycache cache;
    return cache.x(index);
}

constexpr auto global::y() const -> int {
    static xycache cache;
    return cache.y(index);
}

local::local(int subgrid, int index)
    : subgrid(subgrid), index(index)
{}

local::local(int index)
    : local(global(index))
{}

local::local(global c)
    : local(c.index / order2, c.index % order2)
{}

constexpr auto local::x() const -> int {
    return global(*this).x();
}

constexpr auto local::y() const -> int {
    return global(*this).y();
}

// clang-format on

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

class state {
    friend class swap;

  public:
    state();

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

    /// Reads a digit assignment.
    [[nodiscard]]
    auto read_assignment(global c) const -> int;

    /// Overwrites previously assigned digit and adjusts the state accordingly.
    void write_assignment(global c, int digit);

    /// Assigns a digit and adjust the state accordingly.
    void add_assignment(global c, int digit);

    /// Removes previously assigned digit and adjusts the state accordingly.
    void remove_assignment(global c);

    /// Digit frequencies
    ///
    /// Stores the number of digits within rows and columns.
    array<int, order4 + order4> frequencies = {};

    /// ...
    [[nodiscard]]
    auto dec_and_get_frequency(axis ax, int index, int digit) -> int;

    /// ...
    [[nodiscard]]
    auto get_and_inc_frequency(axis ax, int index, int digit) -> int;

    /// Open cells grouped by subgrid.
    ///
    /// Each entry contains the indices of cells that are not fixed by the
    /// puzzle clues and may therefore be modified. Grouped by subgrid to
    /// allow swaps to be performed efficiently while preserving subgrid
    /// validity.
    array<sparse_set<0, order2 - 1>, order2> open_cells;

    /// ...
    void lock(local c);

    /// ...
    void unlock(local c);

    /// ...
    [[nodiscard]]
    auto locked(local c) const -> bool;
};

state::state() {
    for (int i = 0; i < order4; ++i) {
        add_assignment(i, i % order2);
        unlock(i);
    }
}

auto state::load(const filesystem::path &path) -> expected<state, string> {
    std::ifstream input(path);

    if (!input) {
        return unexpected(format("could not open {}", path.string()));
    }

    global k = -1; // coordinate of clue
    local  p;      // coordinate of digit

    int   di;
    char  ch;
    state x;

    while (input.get(ch)) {
        if (ch == ' ' or ch == '\n') {
            continue;
        }

        k = k + 1;

        if (k >= order4) {
            return unexpected("unexpected file size");
        }

        if (ch == '0') {
            continue;
        }

        if (ch >= '1' and ch <= '9') {
            di = ch - '1';
        } else if (ch >= 'a' and ch <= 'z') {
            di = ch - 'a';
        } else if (ch >= 'A' and ch <= 'Z') {
            di = ch - 'A';
        } else {
            return unexpected(format("unexpected token: ", ch));
        }

        if (di < 0 or di >= order2) {
            return unexpected(format("unexpected token: {}", ch));
        }

        p = {local(k).subgrid, di};

        while (x.read_assignment(p) != di) {
            p.index = x.read_assignment(p);
        }

        if (x.locked(p)) {
            return unexpected("unexpected duplicate: " + to_string(ch));
        }

        x.write_assignment(p, x.read_assignment(k));
        x.write_assignment(k, di);
        x.lock(k);
    }

    if (k + 1 != order4) {
        return unexpected("unexpected file size");
    }

    return x;
}

auto state::save(const filesystem::path &path) const -> expected<void, string> {
    std::ofstream output(path);

    if (!output) {
        return unexpected("could not open " + path.string());
    }

    char ch; // NOLINT
    int  di; // NOLINT

    for (int i = 0; i < order4; ++i) {
        di = read_assignment(i);

        if (di < 9) {
            ch = static_cast<char>('1' + di);
        } else {
            ch = static_cast<char>('a' + di);
        }

        output << ch;

        if (i + 1 < order4) {
            output << " ";
        }
    }

    return {};
}

auto state::read_assignment(global c) const -> int {
    return assignments[c];
}

void state::add_assignment(global c, int digit) {
    conflicts += get_and_inc_frequency(axis::x, c.x(), digit);
    conflicts += get_and_inc_frequency(axis::y, c.y(), digit);

    assignments[c] = digit;
}

void state::remove_assignment(global c) {
    int digit = assignments[c];

    conflicts -= dec_and_get_frequency(axis::x, c.x(), digit);
    conflicts -= dec_and_get_frequency(axis::y, c.y(), digit);

    assignments[c] = -1;
}

void state::write_assignment(global c, int digit) {
    remove_assignment(c);
    add_assignment(c, digit);
}

auto state::dec_and_get_frequency(axis ax, int index, int digit) -> int {
    return --frequencies[(static_cast<int>(ax) * order4) + (index * order2) +
                         digit];
}

auto state::get_and_inc_frequency(axis ax, int index, int digit) -> int {
    return frequencies[(static_cast<int>(ax) * order4) + (index * order2) +
                       digit]++;
}

void state::lock(local c) {
    open_cells[c.subgrid].erase(c.index);
}

void state::unlock(local c) {
    open_cells[c.subgrid].insert(c.index);
}

auto state::locked(local c) const -> bool {
    return not open_cells[c.subgrid].contains(c.index);
}

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

    return {subgrid, x.open_cells[subgrid][a], x.open_cells[subgrid][b]};
}

auto swap::apply(state &x) const {
#ifndef NOTHROW
    if (not x.open_cells[subgrid].contains(first) or
        not x.open_cells[subgrid].contains(second)) {
        throw std::invalid_argument("one of swap elements are closed (fixed)");
    }
#endif

    local c1;
    local c2;
    int   a1;
    int   a2;

    c1 = local(subgrid, first);
    c2 = local(subgrid, second);
    a1 = x.read_assignment(c1);
    a2 = x.read_assignment(c2);

    x.write_assignment(c1, a2);
    x.write_assignment(c2, a1);
}

template <typename Iterator>
auto calculate_mean(Iterator begin, Iterator end) -> double {
    using ValueType = iterator_traits<Iterator>::value_type;

    double d   = distance(begin, end);
    double sum = accumulate(begin, end, ValueType{});

    return sum / d;
}

template <typename Iterator>
auto calculate_variance(Iterator begin, Iterator end, double mean) -> double {
    double d = distance(begin, end);
    double sum =
        accumulate(begin, end, 0.0, [mean](double acc, auto x) -> double {
            return acc + pow(static_cast<double>(x) - mean, 2);
        });

    return sum / d;
}

auto run(span<char *> args) -> int {
    using distribution = uniform_real_distribution<double>;

    if (args.size() != 3) {
        cerr << usage(args[0]) << "\n";
        return 0;
    }

    auto *puzzle_file   = args[1];
    auto *solution_file = args[2];

    // load the puzzle file

    auto load_result = state::load(puzzle_file);

    if (not load_result.has_value()) {
        cerr << ERROR << "unable to load " << puzzle_file << "\n";
        cerr << WHY << load_result.error() << "\n";
        return 1;
    }

    auto &x = *load_result;

    cout << INFO << "puzzle loaded\n";
    cout.flush();

    // calculate standard deviation of energy
    // and
    // initialize simulation parameters

    array<int, 1024UL * 100> samples;

    for (int &sample : samples) {
        int  before = x.energy();
        int  after;
        auto swap = swap::random(x);

        swap.apply(x);

        after  = x.energy();
        sample = after - before;
    }

    // run simulated annealing

    cout << INFO << "running simulated annealing...\n";
    cout.flush();

    vector<int> E;

    double mu    = calculate_mean(samples.begin(), samples.end());
    double var   = calculate_variance(samples.begin(), samples.end(), mu);
    double std   = sqrt(var);
    double T0    = std;
    double TN    = 1e-12;
    double alpha = 0.995;
    int    N     = static_cast<int>(log(TN / T0) / log(alpha));
    int    K     = order4 * order4;
    int    S     = N * K;
    int    f     = static_cast<int>(0.01 * S);

    int    diff;
    int    i;
    int    s;
    double T;
    swap   q;

    E.resize(S); // NOLINT

reheat:
    E[0] = x.energy();
    i    = 0;
    s    = 0;
    T    = T0;

freeze:
    if (E[i] == 0) {
        cout << "\r\033[2K";
        goto save; // NOLINT
    }

    if (i % f == 0) {
        cout << "\r\033[2K";
        cout << STATUS;
        cout << static_cast<int>(i / (double)S * 100) << "%";
        cout << ", ";
        cout << "T = " << T;
        cout << ", ";
        cout << "E = " << E[i];

        cout.flush();
    }

    i++;

    if (i == S) {
        goto reheat; // NOLINT
    }

    q = swap::random(x);
    q.apply(x);
    E[i] = x.energy();
    diff = E[i] - E[i - 1];

    if (diff <= 0) {
        s = 0;
    }

    if (diff > 0 and distribution(0.0, 1.0)(rng()) >= exp(-diff / T)) {
        q.apply(x);
        E[i] = E[i - 1];

        if (++s == K) {
            goto reheat; // NOLINT
        }
    }

    if (i % K == 0) {
        T = alpha * T;
    }

    goto freeze; // NOLINT

save:
    if (auto save_result = x.save(solution_file); !save_result) {
        cerr << ERROR << "unable to save solution to " << solution_file << "\n";
        cerr << WHY << save_result.error() << "\n";
        return 1;
    }

    cout << INFO << "solution saved to " << solution_file << "\n";
    return 0;
}

} // namespace

auto main(int argc, char *argv[]) -> int {
    CPPTRACE_TRY {
        return run({argv, static_cast<size_t>(argc)});
    }
    CPPTRACE_CATCH(const exception &e) {
        cerr << "Exception: " << e.what() << "\n";
        cerr << cpptrace::from_current_exception();
    }

    return 0;
}

#endif
