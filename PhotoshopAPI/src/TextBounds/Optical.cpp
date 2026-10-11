#include "Optical.h"
#include <limits>

namespace NAMESPACE_PSAPI::TextBoundsDetail {
namespace {
I fixed(double v) {
  return static_cast<I>(f32(f32(v * 65536) + (v >= 0 ? 0.5 : -0.5)));
}
using Path = std::vector<std::pair<int, DPoint>>;
Path path(Font &font, unsigned gid, I baseline, double yscale = 100) {
  const auto &g = font.outline(gid);
  Path out;
  double scale = f32(1.0 / font.upem),
         origin = fixed(f32(1 - f32(baseline / 100.0))) / 65536.0;
  auto emit = [&](int cmd, DPoint p) {
    out.emplace_back(cmd,
                     DPoint{f32(f32(scale * p[0]) * 1000),
                            f32(f32(origin - f32(scale * p[1])) * yscale)});
  };
  int start = 0;
  for (int end : g.ends) {
    std::vector<std::pair<DPoint, unsigned char>> contour;
    for (int i = end; i >= start; --i)
      contour.emplace_back(g.points[i], g.tags[i]);
    start = end + 1;
    DPoint first;
    std::vector<std::pair<DPoint, unsigned char>> rest;
    if (contour[0].second & 1) {
      first = contour[0].first;
      rest.assign(contour.begin() + 1, contour.end());
      rest.push_back(contour[0]);
    } else if (contour[1].second & 1) {
      first = contour[1].first;
      rest.assign(contour.begin() + 2, contour.end());
      rest.push_back(contour[0]);
      rest.push_back(contour[1]);
      rest.push_back(contour[1]);
    } else {
      first = {(contour[0].first[0] + contour[1].first[0]) / 2,
               (contour[0].first[1] + contour[1].first[1]) / 2};
      rest.assign(contour.begin() + 1, contour.end());
      rest.push_back(contour[0]);
      rest.emplace_back(first, 1);
    }
    emit(0, first);
    DPoint current = first;
    for (size_t i = 0; i < rest.size();) {
      auto [p, flag] = rest[i];
      if (flag & 1) {
        emit(1, p);
        current = p;
        ++i;
        continue;
      }
      auto [n, nflag] = rest[i + 1];
      DPoint target = n;
      if (!(nflag & 1))
        for (int j = 0; j < 2; ++j)
          target[j] = (p[j] + n[j]) / 2;
      DPoint a, b;
      for (int j = 0; j < 2; ++j) {
        a[j] = static_cast<I>((current[j] + (p[j] - current[j]) * 2 / 3) * 64) /
               64.0;
        b[j] = static_cast<I>((target[j] + (p[j] - target[j]) * 2 / 3) * 64) /
               64.0;
      }
      emit(2, a);
      emit(2, b);
      emit(2, target);
      current = target;
      i += (nflag & 1) ? 2 : 1;
    }
    emit(3, first);
  }
  return out;
}
using Box = std::array<I, 4>;
Box bounds(const Path &p) {
  if (p.empty())
    throw std::invalid_argument("font has empty optical calibration outlines");
  double x0 = p[0].second[0], x1 = x0, y0 = p[0].second[1], y1 = y0;
  for (auto [cmd, q] : p) {
    (void)cmd;
    x0 = std::min(x0, q[0]);
    x1 = std::max(x1, q[0]);
    y0 = std::min(y0, q[1]);
    y1 = std::max(y1, q[1]);
  }
  return {static_cast<I>(std::floor(x0)), static_cast<I>(std::floor(y0)),
          static_cast<I>(std::ceil(x1)), static_cast<I>(std::floor(y1)) + 1};
}
std::vector<I> samples(I a, I b, I c, I d, I count) {
  I h = 65536 / count, h2 = i32(h * h) >> 16, h3 = i32(h2 * h) >> 16;
  I cubic = i32(i32(b - c) * 3 - a + d),
    q = floor_div(i32((c - (b - a) - b) * 3) * h2, 65536);
  I step = i32(floor_div(cubic * h3, 65536) + q +
               floor_div(i32((b - a) * 3) * h, 65536));
  I third = floor_div(i32(cubic * 6) * h3, 65536), second = i32(2 * q),
    low = std::min({a, b, c, d});
  std::vector<I> out;
  for (I i = 0; i < count; ++i) {
    second = i32(second + third);
    out.push_back(std::max(a, low));
    a = i32(a + step);
    step = i32(step + second);
  }
  out.push_back(d);
  return out;
}
struct Scan {
  std::array<I, 105> left, right;
  Box box;
  bool edge_only = true;
  Point current{};
  explicit Scan(Box b) : box(b) {
    left.fill(0x7fff0000);
    right.fill(-2147483648LL);
  }
  void update(I row, I a, I b) {
    auto set = [&](auto &arr, I v, bool take) {
      if (take) {
        arr[row] = v;
        if (row != box[1] && row + 1 != box[3] && box[0] != (v >> 16) &&
            box[2] != (i32(v + 65535) >> 16))
          edge_only = false;
      }
    };
    set(left, a, a < left[row]);
    set(right, b, b > right[row]);
  }
  void line(Point end) {
    Point start = current;
    current = end;
    if (start[1] > end[1])
      std::swap(start, end);
    auto [x0, y0] = start;
    auto [x1, y1] = end;
    I first = std::max<I>(y0 >> 16, 0), stop = std::min<I>((y1 >> 16) + 1, 105);
    if (first >= stop)
      return;
    I slope = y1 != y0 ? floor_div(i32(x1 - x0) * 65536, i32(y1 - y0)) : 0,
      row = first, x;
    if ((y0 >> 16) < first)
      x = i32(x0 + floor_div(i32(first * 65536 - y0) * slope, 65536));
    else {
      x = first < (y1 >> 16)
              ? i32(x0 + floor_div((65536 - (y0 & 65535)) * slope, 65536))
              : x1;
      update(row, std::min(x0, x), std::max(x0, x));
      if (++row == stop)
        return;
    }
    while (row < stop - 1) {
      I next = i32(x + slope);
      update(row, std::min(x, next), std::max(x, next));
      x = next;
      ++row;
    }
    I next = stop <= (y1 >> 16) ? i32(x + slope) : x1;
    update(row, std::min(x, next), std::max(x, next));
  }
  void cubic(std::array<Point, 4> points) {
    I ymin = points[0][1], ymax = ymin;
    for (auto p : points) {
      ymin = std::min(ymin, p[1]);
      ymax = std::max(ymax, p[1]);
    }
    I low = std::max<I>(ymin >> 16, 0) * 65536,
      high = std::min<I>((ymax >> 16) + 1, 105) * 65536;
    if (low >= high) {
      for (int i = 1; i < 4; ++i)
        line(points[i]);
      return;
    }
    std::vector<std::pair<std::array<Point, 4>, int>> stack{{points, 0}};
    while (!stack.empty()) {
      auto [p, depth] = stack.back();
      stack.pop_back();
      I span = 0;
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
          span = std::max(span, std::abs(p[i + 1][j] - p[i][j]));
      if ((span >> 16) >= 8 && depth < 15) {
        auto mid = [](Point a, Point b) {
          return Point{i32(a[0] + b[0]) >> 1, i32(a[1] + b[1]) >> 1};
        };
        Point a = mid(p[0], p[1]), b = mid(p[1], p[2]), c = mid(p[2], p[3]),
              d = mid(a, b), e = mid(b, c), f = mid(d, e);
        stack.push_back({{f, e, c, p[3]}, depth + 1});
        stack.push_back({{p[0], a, d, f}, depth + 1});
        continue;
      }
      bool below = true, above = true;
      for (auto q : p) {
        below = below && q[1] < low;
        above = above && q[1] >= high;
      }
      if (below || above) {
        for (int i = 1; i < 4; ++i)
          line(p[i]);
        continue;
      }
      I count = std::min<I>(
          std::max({(span >> 16) * 3,
                    std::abs((p[3][1] >> 16) - (p[0][1] >> 16)), I(2)}),
          25);
      auto xs = samples(p[0][0], p[1][0], p[2][0], p[3][0], count),
           ys = samples(p[0][1], p[1][1], p[2][1], p[3][1], count);
      for (size_t i = 1; i < xs.size(); ++i)
        line({xs[i], ys[i]});
    }
    current = points.back();
  }
};
double down32(double v) {
  float r = static_cast<float>(v);
  if (r > v)
    r = std::nextafter(r, -std::numeric_limits<float>::infinity());
  return r;
}
I rounded(double v) { return static_cast<I>(f32(v + (v >= 0 ? 0.5 : -0.5))); }
I multiply(I a, I b) {
  return rounded(f32(f32(f32(f32(static_cast<double>(a)) / 65536) *
                         f32(static_cast<double>(b))) /
                     65536) *
                 65536);
}
} // namespace
std::optional<Profile> Optical::profile(unsigned gid, unsigned classification,
                                        bool smoothing) {
  auto p = path(face, gid, font[16]);
  if (p.empty())
    return std::nullopt;
  auto box = bounds(p);
  Scan scan(box);
  std::vector<Point> points;
  for (auto [cmd, pt] : p) {
    (void)cmd;
    points.push_back({fixed(pt[0]), fixed(pt[1])});
  }
  Point start{};
  for (size_t i = 0; i < p.size(); ++i) {
    int cmd = p[i].first;
    if (cmd == 0) {
      scan.current = points[i];
      start = points[i];
    } else if (cmd == 1)
      scan.line(points[i]);
    else if (cmd == 2) {
      scan.cubic({scan.current, points[i], points[i + 1], points[i + 2]});
      i += 2;
    } else if (scan.current != start)
      scan.line(start);
  }
  bool missing = false;
  for (I y = std::max<I>(box[1], 0); y < std::min<I>(box[3], 105); ++y)
    missing = missing || scan.left[y] == 0x7fff0000;
  if (!scan.edge_only || missing) {
    --box[0];
    ++box[2];
  }
  I advance = static_cast<I>(std::floor(down32(
      down32(hb_font_get_glyph_h_advance(face.hb, gid) / double(face.upem)) *
      10000)));
  auto [x0, y0, x1, y1] = box;
  Profile result;
  for (int j = 0; j < 4; ++j)
    result.flags[j] =
        static_cast<unsigned char>((classification >> (j * 8)) & 255);
  if (y1 <= y0)
    return result;
  std::array<I, 105> left, right;
  for (size_t i = 0; i < 105; ++i) {
    left[i] = scan.left[i] >> 16;
    right[i] = (scan.right[i] + 65535) >> 16;
  }
  I visible_left = x1, visible_right = x0;
  bool has_rows = false;
  for (I i = std::max<I>(0, y0); i < std::min<I>(101, y1); ++i)
    if (left[i] != 32767) {
      if (!has_rows) {
        visible_left = left[i];
        visible_right = right[i];
        has_rows = true;
      } else {
        visible_left = std::min(visible_left, left[i]);
        visible_right = std::max(visible_right, right[i]);
      }
    }
  result.left.fill((visible_right - visible_left) * 10);
  result.right.fill((visible_right - visible_left) * 10);
  for (I row = 0; row < 100; ++row) {
    I y = 100 - row;
    if (y0 <= y && y < y1 && left[y] != 32767) {
      result.left[row] = (left[y] - x0 - 1) * 10;
      result.right[row] = (x1 - right[y] - 1) * 10;
    }
  }
  I first = 100 - y1, last = 100 - y0;
  if (first < 0 || first > 98)
    first = 0;
  if (last < 1 || last > 99)
    last = 99;
  if (first > last)
    std::swap(first, last);
  result.first = first;
  result.last = last;
  result.left_bearing = (x0 + 1) * 10;
  result.right_bearing = advance - x1 * 10;
  if (smoothing) {
    I base = font[16], height = font[13] - base, slope = 0xe666,
      special = 0x50000;
    if (last < 2 * height / 3 + base || first > height / 3 + base)
      slope = special = 0x28000;
    smooth(result.left, font, result.flags, first, last, slope, special,
           font[0]);
    smooth(result.right, font, result.flags, first, last, slope, special,
           font[0]);
  }
  return result;
}
Optical::Optical(Font &f) : face(f) {
  const std::u32string chars = U"THIxpOo ";
  for (size_t i = 0; i < chars.size(); ++i) {
    auto gid = face.glyph(chars[i]);
    if (!gid)
      throw std::invalid_argument("font lacks optical calibration glyphs");
    font[i + 1] = gid;
  }
  font[10] = hb_face_get_glyph_count(face.face);
  auto hbox = bounds(path(face, static_cast<unsigned>(font[2]), 25, 1000));
  auto xbox = bounds(path(face, static_cast<unsigned>(font[4]), 25, 1000));
  auto pbox = bounds(path(face, static_cast<unsigned>(font[5]), 25, 1000));
  I cap = std::min<I>(750 - hbox[1], 708), xh = std::min<I>(750 - xbox[1], 708),
    descent = std::min(pbox[3] - 700, 1000 - cap), base = descent / 10;
  font[13] = std::min<I>((xh + descent) / 10, 99);
  font[14] = std::min<I>((cap + descent) / 10, 99);
  font[15] = (cap / 10) / 10 + 2 + base;
  font[16] = base;
  auto ip = profile(static_cast<unsigned>(font[3]), 1, false).value();
  I bottom = base + (font[14] - base) / 3,
    top = base + 2 * (font[14] - base) / 3;
  I a = ip.left[top] - ip.left[bottom], b = ip.right[bottom] - ip.right[top];
  I slant = top != bottom ? a * 0x1999 / ((top - bottom) * 10) : 0;
  if (std::abs(a - b) > 20 || slant + 0x3333 < 0 || slant + 0x3333 > 0x10ccc)
    slant = 0;
  font[0] = ((slant * 200 + 0x1000) >> 16) - ((slant * 100 + 0x1000) >> 16);
  std::array<unsigned, 256> table{};
  for (auto r : std::vector<std::array<int, 3>>{{0x30, 0x3a, 1},
                                                {0x41, 0x5b, 1},
                                                {0x61, 0x7b, 2},
                                                {0xc0, 0xdf, 1},
                                                {0xdf, 0x100, 2}})
    for (int c = r[0]; c < r[1]; ++c)
      table[c] = static_cast<unsigned>(r[2]);
  for (int c : {0x8a, 0x8c, 0x8e, 0x9f})
    table[c] = 1;
  for (int c : {0x9a, 0x9c, 0x9e})
    table[c] = 2;
  for (int c : {0x23, 0x26, 0x28, 0x29, 0x2b, 0x2f, 0x3a, 0x3b, 0x3c, 0x3d,
                0x3e, 0x40, 0x5b, 0x5c, 0x5d, 0x7b, 0x7d, 0x85, 0x86, 0x87,
                0x95, 0xa4, 0xa7, 0xb1, 0xb6, 0xb7, 0xd7, 0xf7})
    table[c] = 4;
  for (int c :
       {0x21, 0x24, 0x25, 0x3f, 0x80, 0x83, 0x89, 0xa1, 0xa3, 0xa5, 0xbf})
    table[c] = 5;
  table[0xa2] = 6;
  for (unsigned c = 0; c < 256; ++c) {
    auto gid = face.glyph(static_cast<char32_t>(c));
    unsigned v = table[c];
    if (gid)
      classes[gid] =
          (v & 1) | (((v >> 1) & 1) << 8) | ((v >> 2) & 1) * 0x1010000;
  }
}
const std::optional<Profile> &Optical::cached_profile(unsigned gid) {
  if (auto it = profiles.find(gid); it != profiles.end())
    return it->second;
  if (profiles.size() >= 256)
    profiles.clear();
  return profiles.emplace(gid, profile(gid, classes[gid], gid != font[1]))
      .first->second;
}
std::array<I, 20> Optical::context(double size) {
  if (auto it = contexts.find(size); it != contexts.end())
    return it->second;
  std::array<I, 20> ctx{};
  I sf = static_cast<I>(size * 65536 + 0.5),
    height = (font[14] - font[16]) * 100, weight, offset;
  if (sf <= 0x60000) {
    weight = 0x7333;
    offset = multiply(height, 0x624);
  } else if (sf >= 0x480000) {
    weight = 0x8ccc;
    offset = multiply(height, -0x418);
  } else if (sf < 0xc0000) {
    I fraction = rounded(f32(f32((0xc0000 - sf) / 65536.0) / 6) * 65536);
    weight = 0x8000 - multiply(fraction, 0xccd);
    offset = multiply(fraction, multiply(height, 0x624));
  } else {
    I fraction = rounded(f32(f32((sf - 0xc0000) / 65536.0) / 60) * 65536);
    weight = 0x8000 + multiply(fraction, 0xccc);
    offset = multiply(fraction, multiply(height, -0x418));
  }
  ctx[0] = ctx[1] = offset;
  ctx[7] = weight;
  ctx[10] = 0xe666;
  ctx[11] = 0x50000;
  I space =
      hb_font_get_glyph_h_advance(face.hb, static_cast<unsigned>(font[8]));
  I extra = static_cast<I>(
      f32(f32(f32(space / double(face.upem)) * 0.15258789) * 0.25));
  ctx[4] = extra;
  for (auto r :
       std::vector<std::array<int, 5>>{{6, 1, 3, 9, 14}, {7, 256, 2, 8, 13}}) {
    auto p =
        profile(static_cast<unsigned>(font[r[0]]), static_cast<unsigned>(r[1]))
            .value();
    I sb = p.left_bearing + p.right_bearing;
    ctx[r[3]] = multiply(sb + 2 * offset, 65536 - weight);
    auto left = p.left, right = p.right;
    ctx[r[2]] = gap_statistic(left, right, font[16], font[r[4]]);
    smooth(right, font, p.flags, font[16], font[r[4]], 0x8000, 0x20000,
           font[0]);
    smooth(left, font, p.flags, font[16], font[r[4]], 0x8000, 0x20000,
           -font[0]);
    I stat = gap_statistic(left, right, font[16], font[r[4]]);
    ctx[r[1] == 1 ? 13 : 12] = extra / 5 + 2 * offset + sb + stat;
    if (r[1] == 256)
      ctx[14] = extra / 3 + 2 * offset + sb;
  }
  if (contexts.size() >= 16)
    contexts.clear();
  contexts[size] = ctx;
  return ctx;
}
double Optical::adjustment(unsigned left, unsigned right, double size) {
  // Copy: inserting a new glyph may evict the small profile cache.
  auto lp = cached_profile(left), rp = cached_profile(right);
  if (!lp || !rp)
    return 0;
  I gap = profile_pair(context(size), font, *lp, *rp),
    sb = lp->right_bearing + rp->left_bearing;
  return f32(
      f32((f32(static_cast<double>(gap)) - f32(static_cast<double>(sb))) *
          size) /
      10000);
}
} // namespace NAMESPACE_PSAPI::TextBoundsDetail
