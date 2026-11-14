#include "fixed_containers/algorithm.hpp"

#include "fixed_containers/fixed_vector.hpp"

#include <gtest/gtest.h>

#include <array>
#include <compare>
#include <type_traits>

namespace fixed_containers
{
TEST(Algorithm, EqualUsesSwarForUniqueRepresentations)
{
    static constexpr FixedVector<int, 8> LHS{1, 2, 3, 4};
    static constexpr FixedVector<int, 8> RHS{1, 2, 3, 4};
    static constexpr FixedVector<int, 8> OTHER{1, 2, 3, 5};
    static_assert(LHS == RHS);
    static_assert(LHS != OTHER);

    EXPECT_TRUE(algorithm::equal(LHS.begin(), LHS.end(), RHS.begin(), RHS.end()));
    EXPECT_FALSE(algorithm::equal(LHS.begin(), LHS.end(), OTHER.begin(), OTHER.end()));
}

namespace
{
// Padding-free, so has_unique_object_representations is true, but == is not
// bytewise. The SWAR path must not be used: 1 and 3 compare equal here.
// Constant evaluation already takes the element-wise path; the runtime check
// is what catches a byte-compare fast path.
struct ParityInt
{
    int value;
    constexpr bool operator==(const ParityInt& other) const noexcept
    {
        return (value & 1) == (other.value & 1);
    }
};
}  // namespace

TEST(Algorithm, EqualRespectsCustomEquality)
{
    static_assert(std::has_unique_object_representations_v<ParityInt>);
    static_assert(!algorithm_detail::BytewiseEquality<ParityInt>);

    static constexpr ParityInt ODD_ONE{1};
    static constexpr ParityInt ODD_THREE{3};
    static constexpr ParityInt EVEN{2};
    static_assert(ODD_ONE == ODD_THREE);
    static_assert(ODD_ONE != EVEN);

    static constexpr std::array<ParityInt, 2> LHS{{{1}, {2}}};
    static constexpr std::array<ParityInt, 2> RHS{{{3}, {4}}};
    static constexpr std::array<ParityInt, 2> OTHER{{{1}, {3}}};
    static_assert(algorithm::equal(LHS.begin(), LHS.end(), RHS.begin(), RHS.end()));
    static_assert(!algorithm::equal(LHS.begin(), LHS.end(), OTHER.begin(), OTHER.end()));

    EXPECT_TRUE(algorithm::equal(LHS.begin(), LHS.end(), RHS.begin(), RHS.end()));
    EXPECT_FALSE(algorithm::equal(LHS.begin(), LHS.end(), OTHER.begin(), OTHER.end()));

    const FixedVector<ParityInt, 4> lhs_vec{ParityInt{1}, ParityInt{2}};
    const FixedVector<ParityInt, 4> rhs_vec{ParityInt{3}, ParityInt{4}};
    const FixedVector<ParityInt, 4> other_vec{ParityInt{1}, ParityInt{3}};
    EXPECT_TRUE(lhs_vec == rhs_vec);
    EXPECT_FALSE(lhs_vec == other_vec);

    static constexpr FixedVector<ParityInt, 4> CONST_LHS{ParityInt{1}, ParityInt{2}};
    static constexpr FixedVector<ParityInt, 4> CONST_RHS{ParityInt{3}, ParityInt{4}};
    static_assert(CONST_LHS == CONST_RHS);
}

TEST(Algorithm, LexicographicalCompareUnsignedBytes)
{
    static constexpr std::array<unsigned char, 5> LHS{1, 2, 3, 4, 5};
    static constexpr std::array<unsigned char, 5> RHS{1, 2, 3, 4, 6};
    static_assert(algorithm::lexicographical_compare_three_way(
                      LHS.begin(), LHS.end(), LHS.begin(), LHS.end()) ==
                  std::strong_ordering::equal);
    static_assert(algorithm::lexicographical_compare_three_way(
                      LHS.begin(), LHS.end(), RHS.begin(), RHS.end()) ==
                  std::strong_ordering::less);

    EXPECT_EQ(algorithm::lexicographical_compare_three_way(
                  LHS.begin(), LHS.end(), RHS.begin(), RHS.end()),
              std::strong_ordering::less);
    EXPECT_EQ(algorithm::lexicographical_compare_three_way(
                  RHS.begin(), RHS.end(), LHS.begin(), LHS.end()),
              std::strong_ordering::greater);
}

TEST(Algorithm, LexicographicalCompareIntsUnchanged)
{
    static constexpr std::array LHS{1, 2, 3};
    static constexpr std::array RHS{1, 2, 4};
    static_assert(algorithm::lexicographical_compare_three_way(
                      LHS.begin(), LHS.end(), RHS.begin(), RHS.end()) ==
                  std::strong_ordering::less);
}
}  // namespace fixed_containers
