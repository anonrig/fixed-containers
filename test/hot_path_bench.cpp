// Reproducible kernels for the hot-path instruction-count and timing numbers.
//
// Timing (median ns/op), from a Release / -O3 build:
//   bazel run //:hot_path_bench
//   ./build/hot_path_bench --benchmark_min_time=0.5s
//
// Instruction counts (static size of each noinline kernel, the figure quoted
// on the PR). Disassemble the fc_kernel_* symbols and exclude `endbr64` and
// alignment nops. Example:
//
//   c++ -std=c++20 -O3 -DNDEBUG -fno-exceptions -c test/hot_path_bench.cpp \
//       -I include -isystem /path/to/benchmark/include -o /tmp/hot_path_bench.o
//   objdump -d /tmp/hot_path_bench.o
//
// Building this target at -O3 and running `objdump -d` on the binary works
// too (`nm -C hot_path_bench | grep fc_kernel_`).
//
// The fc_kernel_* functions are marked noinline and have external linkage so
// their generated code stays attributable and is not absorbed into the
// benchmark loop.

#include "fixed_containers/algorithm.hpp"
#include "fixed_containers/circular_indexing.hpp"
#include "fixed_containers/fixed_unordered_map.hpp"
#include "fixed_containers/fixed_vector.hpp"
#include "fixed_containers/integer_range.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>

// gcc/clang attribute; MSVC uses a declspec. The instruction-count recipe
// above is for gcc/clang, but the binary still has to build on MSVC (CI).
#if defined(_MSC_VER)
#define FC_KERNEL_NOINLINE __declspec(noinline)
#else
#define FC_KERNEL_NOINLINE [[gnu::noinline]]
#endif

namespace fixed_containers::hot_path_bench
{
using Map32 = FixedUnorderedMap<int, int, 32>;
using Vec128 = FixedVector<int, 128>;

// Default 32-entry FixedUnorderedMap lookup. Hits an inserted key.
FC_KERNEL_NOINLINE int fc_kernel_unordered_map_find(const Map32& map, int key)
{
    const auto iter = map.find(key);
    if (iter == map.end())
    {
        return -1;
    }
    return (*iter).second;
}

// uninitialized_relocate of 32 ints (contiguous trivially-copyable range).
FC_KERNEL_NOINLINE int* fc_kernel_uninitialized_relocate_32(int* first, int* dest)
{
    return algorithm::uninitialized_relocate(first, first + 32, dest);
}

// FixedVector insert+erase at begin()+1. Caller must leave at least one free
// slot (insert on a full FixedVector is undefined). The erase removes the
// element that was just inserted, so the vector contents are unchanged.
FC_KERNEL_NOINLINE void fc_kernel_vector_insert_erase(Vec128& values)
{
    values.insert(values.begin() + 1, 0);
    values.erase(values.begin() + 1);
}

// wrap_decrement / wrap_increment over [0, 16).
FC_KERNEL_NOINLINE circular_indexing::CyclesAndInteger fc_kernel_wrap_decrement(std::size_t index,
                                                                                std::size_t n)
{
    static constexpr IntegerRange RANGE = IntegerRange::closed_open(0, 16);
    return circular_indexing::decrement_index_with_wraparound(RANGE, index, n);
}

FC_KERNEL_NOINLINE circular_indexing::CyclesAndInteger fc_kernel_wrap_increment(std::size_t index,
                                                                                std::size_t n)
{
    static constexpr IntegerRange RANGE = IntegerRange::closed_open(0, 16);
    return circular_indexing::increment_index_with_wraparound(RANGE, index, n);
}
}  // namespace fixed_containers::hot_path_bench

namespace
{
using fixed_containers::hot_path_bench::fc_kernel_uninitialized_relocate_32;
using fixed_containers::hot_path_bench::fc_kernel_unordered_map_find;
using fixed_containers::hot_path_bench::fc_kernel_vector_insert_erase;
using fixed_containers::hot_path_bench::fc_kernel_wrap_decrement;
using fixed_containers::hot_path_bench::fc_kernel_wrap_increment;
using fixed_containers::hot_path_bench::Map32;
using fixed_containers::hot_path_bench::Vec128;

void BM_UnorderedMapFindDefault32(benchmark::State& state)
{
    Map32 map{};
    for (int i = 0; i < 32; ++i)
    {
        map.try_emplace(i, i * 3);
    }
    for (auto _ : state)
    {
        int value = fc_kernel_unordered_map_find(map, 7);
        benchmark::DoNotOptimize(value);
    }
}

void BM_UninitializedRelocate32(benchmark::State& state)
{
    std::array<int, 64> src{};
    std::array<int, 64> dst{};
    for (int i = 0; i < 64; ++i)
    {
        src[static_cast<std::size_t>(i)] = i;
    }
    for (auto _ : state)
    {
        // Source and destination do not overlap. The trivial path memmoves and
        // does not destroy the source, so `src` stays intact across iterations.
        int* out = fc_kernel_uninitialized_relocate_32(src.data(), dst.data());
        benchmark::DoNotOptimize(out);
        benchmark::DoNotOptimize(dst);
    }
}

void BM_VectorInsertErase(benchmark::State& state)
{
    Vec128 values{};
    // 127 of 128 slots filled so insert has a free slot and the relocated
    // tail is long.
    for (int i = 0; i < 127; ++i)
    {
        values.push_back(i);
    }
    for (auto _ : state)
    {
        fc_kernel_vector_insert_erase(values);
        benchmark::ClobberMemory();
    }
}

void BM_WrapDecrement(benchmark::State& state)
{
    std::size_t index = 3;
    for (auto _ : state)
    {
        auto result = fc_kernel_wrap_decrement(index, 5);
        benchmark::DoNotOptimize(result);
        index = result.integer;
    }
}

void BM_WrapIncrement(benchmark::State& state)
{
    std::size_t index = 14;
    for (auto _ : state)
    {
        auto result = fc_kernel_wrap_increment(index, 5);
        benchmark::DoNotOptimize(result);
        index = result.integer;
    }
}

BENCHMARK(BM_UnorderedMapFindDefault32);
BENCHMARK(BM_UninitializedRelocate32);
BENCHMARK(BM_VectorInsertErase);
BENCHMARK(BM_WrapDecrement);
BENCHMARK(BM_WrapIncrement);
}  // namespace

BENCHMARK_MAIN();
