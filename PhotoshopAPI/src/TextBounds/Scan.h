#pragma once
#include "Macros.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace NAMESPACE_PSAPI::TextBoundsDetail {
using I = int64_t;
using Point = std::array<I, 2>;
using Span = std::array<I, 3>;
inline I i32(I n) { return std::bit_cast<int32_t>(static_cast<uint32_t>(n)); }
inline I floor_div(I a, I b) {
  I q = a / b;
  return q - (a % b != 0 && ((a < 0) != (b < 0)));
}
inline double f32(double n) {
  return static_cast<double>(static_cast<float>(n));
}
std::vector<Span> scan(const std::vector<Point> &points,
                       const std::vector<unsigned char> &tags,
                       const std::vector<int> &ends);
} // namespace NAMESPACE_PSAPI::TextBoundsDetail
