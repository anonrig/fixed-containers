#pragma once

#include "fixed_containers/assert_or_abort.hpp"
#include "fixed_containers/int_math.hpp"
#include "fixed_containers/integer_range.hpp"

#include <bit>
#include <compare>
#include <cstdint>
#include <tuple>

namespace fixed_containers::circular_indexing
{
struct CyclesAndInteger
{
    std::int64_t cycles;
    std::size_t integer;

    constexpr bool operator==(const CyclesAndInteger&) const noexcept = default;
    constexpr std::strong_ordering operator<=>(const CyclesAndInteger& other) const noexcept
    {
        return std::tie(cycles, integer) <=> std::tie(other.cycles, other.integer);
    }
};

template <IsIntegerRange IntegerRangeType>
constexpr CyclesAndInteger increment_index_with_wraparound(const IntegerRangeType& range,
                                                           std::size_t index,
                                                           std::size_t n)
{
    const std::size_t range_size = range.distance();
    if (range_size == 0)
    {
        // Default constructed
        assert_or_abort(index == 0);
        return {};
    }

    const std::size_t start = range.start_inclusive();
    // Fast path for the common [0, N) window (FixedDeque's storage range).
    //
    // The general path below has to shift an arbitrary start offset through
    // safe_subtract before it can divide. When the window is [0, N) that
    // adjustment is the identity, so one add is enough:
    //   cycles  = (index + n) / N
    //   integer = (index + n) % N
    // That drops the subtract, the widening cast, and the separate
    // remainder-via-multiply, which is why this path is shorter.
    //
    // If N is a power of two the quotient/remainder are a shift and a mask.
    // Spelling that explicitly matters when N is only known at runtime: the
    // compiler will not invent an "if power-of-two then AND" specialization.
    // When N is a compile-time constant power of two, gcc/clang already
    // strength-reduce the `/` `%` form to the same shift/AND; the explicit
    // form matches that and is what FixedDeque uses for its own wrap.
    if (start == 0)
    {
        if (std::has_single_bit(range_size))
        {
            return {
                .cycles = static_cast<std::int64_t>((index + n) >> std::countr_zero(range_size)),
                .integer = (index + n) & (range_size - 1)};
        }
        return {.cycles = static_cast<std::int64_t>((index + n) / range_size),
                .integer = (index + n) % range_size};
    }

    const std::size_t new_index_unwrapped = index + n;
    const auto adjust_to_zero =
        int_math::safe_subtract(new_index_unwrapped, start).template cast<std::size_t>();
    const std::size_t quotient = adjust_to_zero / range_size;
    const std::size_t remainder = adjust_to_zero - (quotient * range_size);
    return {.cycles = static_cast<int64_t>(quotient), .integer = remainder + start};
}

template <IsIntegerRange IntegerRangeType>
constexpr CyclesAndInteger decrement_index_with_wraparound(const IntegerRangeType& range,
                                                           std::size_t index,
                                                           std::size_t n)
{
    const std::size_t range_size = range.distance();
    if (range_size == 0)
    {
        // Default constructed
        assert_or_abort(index == 0);
        return {};
    }

    const std::size_t start = range.start_inclusive();
    // Fast path for [0, N). The general path turns a decrement into an
    // increment by a complementary offset: add one extra cycle, step forward
    // by `extra * N - n`, then subtract that extra cycle. That is correct for
    // an arbitrary start, but it is a longer dependency chain (two divisions,
    // a multiply, safe_subtract, then a full increment).
    //
    // For a window that starts at 0 the residue is a plain modulo, so the
    // result can be computed directly. Split both operands into cycles and
    // residues:
    //   index = (index / N) * N + (index % N)
    //   n     = (n / N) * N + (n % N)
    // If n's residue fits in index's residue, subtract it and keep
    // `index/N - n/N` cycles. Otherwise wrap around the window once:
    // integer = index%N + N - n%N, and one additional negative cycle.
    //
    // `index` may sit far outside [0, N) -- FixedDeque's virtual origin is
    // near SIZE_MAX/2 -- so both operands are reduced mod N before the
    // subtract. That keeps `integer` in range without a negative intermediate
    // (which size_t cannot represent).
    //
    // A runtime power-of-two N still goes through `%`/`/` here; the compiler
    // will not emit a mask/shift unless it can see that N is constant, in
    // which case it strength-reduces these operations the same way as the
    // increment path. FixedDeque bypasses this function entirely for
    // compile-time power-of-two capacities and uses `(index - n) & (N - 1)`.
    if (start == 0)
    {
        const std::size_t index_mod = index % range_size;
        const std::size_t n_mod = n % range_size;
        const std::size_t n_cycles = n / range_size;
        if (n_mod <= index_mod)
        {
            return {.cycles = static_cast<std::int64_t>(index / range_size) -
                              static_cast<std::int64_t>(n_cycles),
                    .integer = index_mod - n_mod};
        }
        return {.cycles = static_cast<std::int64_t>(index / range_size) -
                          static_cast<std::int64_t>(n_cycles + 1),
                .integer = index_mod + range_size - n_mod};
    }

    const std::size_t negative_cycles = (n / range_size) + 1;
    const auto positive_modulo =
        int_math::safe_subtract(negative_cycles * range_size, n).template cast<std::size_t>();

    CyclesAndInteger out = increment_index_with_wraparound(range, index, positive_modulo);
    out.cycles -= static_cast<std::int64_t>(negative_cycles);
    return out;
}
}  // namespace fixed_containers::circular_indexing
