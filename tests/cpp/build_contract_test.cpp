#include <climits>
#include <cstdint>
#include <limits>
#include <numeric>

namespace {

constexpr std::int64_t kRationalComponentMax = 2'147'483'647LL;

static_assert(CHAR_BIT == 8, "Stage 0-E baseline requires 8-bit bytes");
static_assert(sizeof(std::int64_t) == 8, "Stage 0-E baseline requires exact 64-bit int64_t");
static_assert(std::numeric_limits<std::int64_t>::is_signed);
static_assert(std::numeric_limits<std::uint64_t>::digits == 64);
static_assert(__cplusplus >= 202002L, "ST Music Workstation requires C++20");

constexpr std::int64_t kMaxComponentProduct =
    kRationalComponentMax * kRationalComponentMax;

constexpr std::int64_t kMaxAdditionIntermediate =
    kMaxComponentProduct + kMaxComponentProduct;

static_assert(kMaxComponentProduct > 0);
static_assert(kMaxComponentProduct <= std::numeric_limits<std::int64_t>::max());
static_assert(kMaxAdditionIntermediate <= std::numeric_limits<std::int64_t>::max());
static_assert(std::gcd(kRationalComponentMax, kRationalComponentMax - 1) == 1);

} // namespace

int main()
{
    return 0;
}
