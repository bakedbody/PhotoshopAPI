#pragma once
#include "Font.h"
#include <optional>

namespace NAMESPACE_PSAPI::TextBoundsDetail {
using Rows = std::array<I, 100>;
using Flags = std::array<unsigned char, 4>;
struct Profile {
  Rows left{}, right{};
  I left_bearing = 0, right_bearing = 0, first = 0, last = 0;
  Flags flags{};
};
struct Optical {
  Font &face;
  std::array<I, 22> font{};
  std::map<unsigned, unsigned> classes;
  std::map<unsigned, std::optional<Profile>> profiles;
  std::map<double, std::array<I, 20>> contexts;
  explicit Optical(Font &f);
  std::optional<Profile> profile(unsigned gid, unsigned classification,
                                 bool smoothing = true);
  const std::optional<Profile> &cached_profile(unsigned gid);
  std::array<I, 20> context(double size);
  double adjustment(unsigned left, unsigned right, double size);
};
I gap_statistic(const Rows &left, const Rows &right, I start, I end);
void smooth(Rows &values, const std::array<I, 22> &font, const Flags &flags,
            I first, I last, I slope, I special, I italic);
I profile_pair(const std::array<I, 20> &context, const std::array<I, 22> &font,
               const Profile &left, const Profile &right);
} // namespace NAMESPACE_PSAPI::TextBoundsDetail
