#include "Optical.h"
#include <limits>

namespace NAMESPACE_PSAPI::TextBoundsDetail {
I gap_statistic(const Rows &left, const Rows &right, I start, I end) {
  start = std::max<I>(start, 0);
  end = std::min<I>(end, 99);
  if (start > end)
    return 0;
  I total = 0;
  for (I i = start; i <= end; ++i) {
    I gap = i32(left[i] + right[i]), distance = std::abs(gap);
    if (distance <= 19990)
      distance = ((distance + 5) / 10) * 10;
    I root = static_cast<I>(
        std::floor(std::sqrt(static_cast<double>(distance)) * 65536 + 0.5));
    total = i32(total + (gap >= 0 ? root : -root));
  }
  I avg = total / ((end - start + 1) * 65536);
  return i32(avg * std::abs(avg));
}
namespace {
I constrain(const Rows &left, const Rows &right, I start, I end, I minimum,
            I result) {
  start = std::max<I>(start, 0);
  end = std::min<I>(end, 99);
  if (start > end)
    return result;
  I gap = std::numeric_limits<I>::max();
  for (I i = start; i <= end; ++i)
    for (I j = std::max(start, i - 1); j <= std::min(end, i + 1); ++j)
      gap = std::min(gap, left[i] + right[j]);
  return std::max(result, minimum - gap);
}
I collision(const Rows &left, const Rows &right, const std::array<I, 22> &font,
            const Flags &fl, const Flags &fr, I first_l, I last_l, I first_r,
            I last_r, I first, I last, I minimum, I outer, I result) {
  I low = std::max<I>(first, 0), high = std::min<I>(last, 99);
  I baseline = font[0x40 / 4], cap = font[0x34 / 4], asc = font[0x38 / 4],
    shoulder = font[0x3c / 4];
  I pad = shoulder - baseline, bottom = baseline - pad, cap_lo = cap - pad,
    cap_hi = cap + pad, asc_lo = asc - pad, asc_hi = asc + pad;
  bool mixed = !((fl[0] && fr[0]) || (fl[1] && fr[1]));
  if (result < 0)
    --result;
  I first_gap = left[low] + std::min(right[low], right[low + 1]);
  result = std::max(result, minimum - first_gap);
  if (mixed && (first_l < bottom || first_r < bottom || shoulder < low) &&
      (first_l < asc_lo || first_r < asc_lo || asc_hi < low))
    result = std::max(result, outer - first_gap);
  I last_gap = left[high] + std::min(right[high], right[high - 1]);
  result = std::max(result, minimum - last_gap);
  if (mixed && (shoulder < last_l || shoulder < last_r || high < bottom) &&
      (cap_hi < last_l || cap_hi < last_r || high < cap_lo) &&
      (asc_hi < last_l || asc_hi < last_r || high < asc_lo))
    result = std::max(result, outer - last_gap);
  ++low;
  --high;
  if (minimum == outer)
    return constrain(left, right, low, high, minimum, result);
  result =
      constrain(left, right, low, std::min(high, bottom - 1), minimum, result);
  if (mixed && (shoulder + 1 < last_l || shoulder + 1 < last_r)) {
    I margin = first_l < bottom || first_r < bottom ? outer : minimum;
    result = constrain(left, right, std::max(low, bottom),
                       std::min(high, shoulder), margin, result);
    result = constrain(left, right, std::max(low, shoulder + 1),
                       std::min(high, cap_lo - 1), outer, result);
    if (!(fl[1] && fr[1])) {
      margin = cap_hi < last_l || cap_hi < last_r ? outer : minimum;
      result = constrain(left, right, std::max(low, cap_lo),
                         std::min(high, cap_hi), margin, result);
      result = constrain(left, right, std::max(low, cap_hi + 1),
                         std::min(high, asc_lo - 1), outer, result);
      if (!(fl[0] && fr[0])) {
        margin = last_l <= asc_hi && last_r <= asc_hi ? minimum : outer;
        result = constrain(left, right, std::max(low, asc_lo),
                           std::min(high, asc_hi), margin, result);
        result = constrain(left, right, std::max(low, asc_hi + 1), high, outer,
                           result);
      }
    }
  }
  return result;
}
void extend(Rows &p, const std::array<I, 22> &font, I first, I last, I italic) {
  I baseline = font[16], height = font[13] - baseline;
  if (last < 2 * height / 3 + baseline && last >= 0)
    for (I i = last + 1; i < 100; ++i)
      p[i] = std::min(p[i], p[i - 1] + 20 + italic);
  if (height / 3 + baseline < first && first < 100)
    for (I i = first - 1; i >= 0; --i)
      p[i] = std::min(p[i], p[i + 1] + 20 + italic);
}
} // namespace
I profile_pair(const std::array<I, 20> &ctx, const std::array<I, 22> &font,
               const Profile &lp, const Profile &rp) {
  Rows left = lp.right, right = rp.left;
  I fl = lp.first, ll = lp.last, fr = rp.first, lr = rp.last;
  I baseline = font[16], height = font[13] - baseline;
  bool direct = (std::min(ll, lr) > 2 * height / 3 + baseline &&
                 std::max(fl, fr) <= height / 3 + baseline) ||
                (std::min(fl, fr) == 0 && std::max(ll, lr) == 99);
  if (!direct) {
    extend(left, font, fl, ll, -font[0]);
    extend(right, font, fr, lr, font[0]);
  }
  I low = std::max({fl, fr, I(0)}), high = std::min({ll, lr, I(99)}),
    pad = 2 * (font[15] - baseline);
  if (high - pad < low)
    low = std::max<I>(high - pad, 0);
  if (high < std::max({fl, fr, I(0)}) + pad)
    high = std::min<I>(std::max({fl, fr, I(0)}) + pad, 99);
  I end = std::min(font[14], high), start = low;
  const auto &flags_l = lp.flags;
  const auto &flags_r = rp.flags;
  if (flags_l[1] || flags_r[1]) {
    end = std::min(end, font[13]);
    start = std::max(start, baseline);
  } else if (flags_l[0] || flags_r[0])
    start = std::max(start, baseline);
  I stat = gap_statistic(left, right, start, end), target = ctx[2];
  if (flags_l[0] || flags_r[0]) {
    if (!(flags_l[1] || flags_r[1]))
      target = ctx[3];
    if ((flags_l[0] && flags_r[1]) || (flags_r[0] && flags_l[1]))
      target = (ctx[3] + target) / 2;
  }
  I adjustment = target - stat;
  if (flags_r[3] || flags_l[2])
    adjustment += ctx[4];
  I result = collision(left, right, font, flags_l, flags_r, fl, ll, fr, lr, low,
                       high, ctx[5], ctx[6], adjustment);
  I weight = ctx[7], sidebearings = lp.right_bearing + rp.left_bearing;
  if (weight) {
    double a = f32(f32(f32(static_cast<double>(weight)) / 65536) *
                   f32(static_cast<double>(result))) /
               65536;
    double b = f32(f32(f32(static_cast<double>(65536 - weight)) / 65536) *
                   f32(static_cast<double>(sidebearings))) /
               65536;
    result = static_cast<I>(f32(a * 65536 + (a >= 0 ? 0.5 : -0.5))) +
             static_cast<I>(f32(b * 65536 + (b >= 0 ? 0.5 : -0.5))) +
             ctx[flags_l[1] ? 8 : 9] / 2 + ctx[flags_r[1] ? 8 : 9] / 2;
  }
  return result;
}
void smooth(Rows &values, const std::array<I, 22> &font, const Flags &flags,
            I first, I last, I slope, I special, I italic) {
  I cap = font[13], asc = font[14], shoulder = font[15], base = font[16];
  first = std::max<I>(first, 0);
  last = std::min<I>(last, 99);
  I pad = shoulder - base;
  for (I row = std::max(base, first) + 1; row < 100; ++row) {
    I amount = slope;
    if (base - pad - 1 <= row && row <= shoulder + 3 &&
        ((base - pad - 1 <= first && first <= shoulder) || flags[0] ||
         flags[1]))
      amount = special;
    I candidate = values[row - 1] + (i32(amount * 100 + 0x1000) >> 16) + italic;
    if (candidate < values[row] || row > last)
      values[row] = candidate;
  }
  for (I row = std::min(last, asc) - 1; row >= 0; --row) {
    I amount = slope;
    if ((asc - pad - 3 <= row && row <= asc + 1 + pad &&
         ((asc - pad <= last && last <= asc + 1 + pad) || flags[0])) ||
        (cap - pad - 3 <= row && row <= cap + 1 + pad &&
         ((cap - pad <= last && last <= cap + 1 + pad) || flags[1])))
      amount = special;
    I candidate = values[row + 1] + (i32(amount * 100 + 0x1000) >> 16) - italic;
    if (candidate < values[row] || row < first)
      values[row] = candidate;
  }
}
} // namespace NAMESPACE_PSAPI::TextBoundsDetail
