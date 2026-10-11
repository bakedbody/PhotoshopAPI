#include "Scan.h"
#include <map>
#include <stdexcept>

namespace NAMESPACE_PSAPI::TextBoundsDetail {
namespace {
I muldiv(I a, I b, I c) {
  I n = a * b;
  return ((n < 0) != (c < 0) ? -1 : 1) *
         ((std::abs(n) + std::abs(c) / 2) / std::abs(c));
}
Point mid(Point a, Point b, bool ceil = false) {
  return {(a[0] + b[0] + ceil) >> 1, (a[1] + b[1] + ceil) >> 1};
}
using Piece = std::vector<Point>;
std::vector<Point> line(Point p, Point q) {
  auto [x0, y0] = p;
  auto [x1, y1] = q;
  if (y0 == y1)
    return {};
  I down_y = y1 < y0, sy = down_y ? -1 : 1;
  I yl = down_y ? ((y0 - 33) & ~I(63)) + 32 : ((y0 + 32) & ~I(63)) + 32;
  I y = yl >> 6,
    ny = down_y ? y - ((y1 + 32) >> 6) + 1 : ((y1 - 33) >> 6) - y + 1;
  I down_x = x1 < x0, sx = down_x ? -1 : 1;
  I xl = down_x ? ((x0 - 33) & ~I(63)) + 32 : ((x0 + 32) & ~I(63)) + 32;
  I x = xl >> 6,
    nx = down_x ? x - ((x1 + 32) >> 6) + 1 : ((x1 - 33) >> 6) - x + 1;
  std::vector<Point> out;
  if (x0 == x1) {
    x = (x0 - I(y1 > y0) + 32) >> 6;
    for (I k = 0; k < ny; ++k)
      out.push_back({x, y + k * sy});
    return out;
  }
  I dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
  I error =
      i32((down_x ^ down_y) + dx * std::abs(yl - y0) - std::abs(xl - x0) * dy);
  x += down_x;
  for (I k = 0; k < nx + ny; ++k) {
    if (error <= 0) {
      out.push_back({x, y});
      y += sy;
      error = i32(error + dx * 64);
    } else {
      x += sx;
      error = i32(error - dy * 64);
    }
  }
  return out;
}
std::vector<Point> monotone(const Piece &p) {
  auto [x0, y0] = p[0];
  auto [x1, y1] = p[1];
  auto [x2, y2] = p[2];
  I down_y = y2 <= y0, down_x = x2 <= x0, sy = down_y ? -1 : 1,
    sx = down_x ? -1 : 1;
  I yl = down_y ? ((y0 - 33) & ~I(63)) + 32 : ((y0 + 32) & ~I(63)) + 32;
  I xl = down_x ? ((x0 - 33) & ~I(63)) + 32 : ((x0 + 32) & ~I(63)) + 32;
  I ye = down_y ? ((y2 + 32) >> 6) - 1 : ((y2 - 33) >> 6) + 1;
  I xe = down_x ? ((x2 + 32) >> 6) - 1 : ((x2 - 33) >> 6) + 1;
  I x = xl >> 6, y = yl >> 6;
  std::vector<Point> out;
  if (y == ye)
    return out;
  if (x == xe) {
    for (I yy = y; yy != ye; yy += sy)
      out.push_back({x + down_x, yy});
    return out;
  }
  I px = std::abs(xl - x0), py = std::abs(yl - y0), cx = std::abs(x1 - x0),
    cy = std::abs(y1 - y0);
  I dx = std::abs(x2 - x0), dy = std::abs(y2 - y0),
    det = 2 * (cx * dy - dx * cy);
  auto bits = [](I v) {
    return static_cast<int>(std::bit_width(static_cast<uint64_t>(std::abs(v))));
  };
  int max_bits = bits(std::max(dx, dy));
  static constexpr int table[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3};
  int index = bits(det) + max_bits;
  if (index >= 38)
    throw std::runtime_error("curve scan precision overflow");
  int shift = table[index];
  if (shift) {
    I half = I(1) << (shift - 1);
    for (auto v : {&px, &py, &cx, &cy, &dx, &dy})
      *v = (*v + half) >> shift;
    det = 2 * (cx * dy - dx * cy);
  }
  int precision = 6 - shift;
  I unit = I(1) << precision;
  I ax = dx - 2 * cx, ay = dy - 2 * cy, yy = ay * ay, xx = ax * ax,
    cross = -ay * ax;
  I ex, ey, tx, ty, exy, exx, eyy;
  if (max_bits >= 8) {
    ex = i32((px + unit / 2) * yy + cross * py + det * cy);
    ey = i32((py + unit / 2) * xx + cross * px - det * cx);
    tx = i32((yy >> 1) * px + cross * py + det * cy) >> precision;
    ty = i32((xx >> 1) * py - det * cx) >> precision;
    exy = i32(cross * unit);
    exx = i32(yy * (unit / 2));
    eyy = i32(xx * (unit / 2));
  } else {
    cross *= 2;
    ex = i32(((unit + px * 2) * yy + det * cy * 2 + cross * py) * unit);
    ey = i32((cross * px + (unit + py * 2) * xx - det * cx * 2) * unit);
    tx = i32(yy * px + det * cy * 2 + cross * py);
    ty = i32(xx * py - det * cx * 2);
    exy = i32(cross * unit * unit);
    exx = i32(yy * unit * unit);
    eyy = i32(xx * unit * unit);
  }
  I error = i32(tx * px + ty * py + (down_x ^ down_y));
  x += down_x;
  xe += down_x;
  while (x != xe && y != ye) {
    bool step_y = det < 1 ? (error < 0 || exx < ex) : !(error < 0 || eyy < ey);
    if (step_y) {
      out.push_back({x, y});
      y += sy;
      error = i32(error + ey);
      ex = i32(ex + exy);
      ey = i32(ey + 2 * eyy);
    } else {
      x += sx;
      if (det > 0 && x == xe)
        break;
      error = i32(error + ex);
      ex = i32(ex + 2 * exx);
      ey = i32(ey + exy);
    }
  }
  while (y != ye) {
    out.push_back({x, y});
    y += sy;
  }
  return out;
}
void split(Point p, Point c, Point q, std::vector<Piece> &out) {
  I dx0 = c[0] - p[0], dy0 = c[1] - p[1], dx1 = q[0] - c[0], dy1 = q[1] - c[1];
  if (dy0 * dy1 < 0) {
    I den = dy0 - dy1, xa = p[0] + muldiv(dx0, dy0, den),
      xb = c[0] + muldiv(dx1, dy0, den);
    Point m = {xa + muldiv(xb - xa, dy0, den), p[1] + muldiv(dy0, dy0, den)};
    split(p, {xa, m[1]}, m, out);
    split(m, {xb, m[1]}, q, out);
  } else if (dx0 * dx1 < 0) {
    I den = dx0 - dx1, ya = p[1] + muldiv(dy0, dx0, den),
      yb = c[1] + muldiv(dy1, dx0, den);
    Point m = {p[0] + muldiv(dx0, dx0, den), ya + muldiv(yb - ya, dx0, den)};
    split(p, {m[0], ya}, m, out);
    split(m, {m[0], yb}, q, out);
  } else if (std::abs(q[0] - p[0]) > 3200 || std::abs(q[1] - p[1]) > 3200) {
    Point a = mid(p, c), b = mid(c, q), m = mid(a, b);
    split(p, a, m, out);
    split(m, b, q, out);
  } else if (dx0 * dy1 == dy0 * dx1)
    out.push_back({p, q});
  else
    out.push_back({p, c, q});
}
std::vector<Span> vertices(Point p, Point c, Point n) {
  auto [px, py] = p;
  auto [x, y] = c;
  auto [nx, ny] = n;
  if ((y & 63) != 32)
    return {};
  Span left = {(x + 31) >> 6, y >> 6, 1}, right = {(x + 32) >> 6, y >> 6, -1};
  if (y < ny) {
    if (py < y)
      return {left};
    if (y < py)
      return {left, right};
    return px > x ? std::vector<Span>{left} : std::vector<Span>{};
  }
  if (ny < y) {
    if (py < y)
      return {left, right};
    if (y < py)
      return {right};
    return x > px ? std::vector<Span>{right} : std::vector<Span>{};
  }
  if (y < py)
    return x > nx ? std::vector<Span>{right} : std::vector<Span>{};
  if (py == y) {
    if (px < x && nx < x)
      return {right};
    if (px <= x)
      return {};
  }
  return nx > x ? std::vector<Span>{left} : std::vector<Span>{};
}
} // namespace
std::vector<Span> scan(const std::vector<Point> &points,
                       const std::vector<unsigned char> &tags,
                       const std::vector<int> &ends) {
  std::map<I, std::vector<std::pair<I, I>>> rows;
  int start = 0;
  for (int end : ends) {
    std::vector<std::pair<Point, bool>> expanded;
    for (int i = start; i <= end; ++i) {
      int j = i == end ? start : i + 1;
      bool on = tags[i] & 1, next = tags[j] & 1;
      expanded.emplace_back(points[i], on);
      if (!on && !next)
        expanded.emplace_back(mid(points[i], points[j], true), true);
    }
    start = end + 1;
    auto first = std::find_if(expanded.begin(), expanded.end(),
                              [](const auto &v) { return v.second; });
    std::rotate(expanded.begin(), first, expanded.end());
    expanded.push_back(expanded.front());
    std::vector<Piece> pieces;
    for (size_t i = 0; i + 1 < expanded.size();) {
      auto [c, on] = expanded[i + 1];
      Point p = expanded[i].first;
      if (on) {
        pieces.push_back({p, c});
        ++i;
      } else {
        split(p, c, expanded[i + 2].first, pieces);
        i += 2;
      }
    }
    for (const auto &piece : pieces) {
      I sign = piece.back()[1] > piece.front()[1] ? 1 : -1;
      auto xs = piece.size() == 2 ? line(piece[0], piece[1]) : monotone(piece);
      for (auto [x, y] : xs)
        rows[y].emplace_back(x, sign);
    }
    std::vector<Point> vs;
    for (const auto &piece : pieces)
      if (vs.empty() || vs.back() != piece[0])
        vs.push_back(piece[0]);
    if (vs.size() > 1 && vs.back() == vs.front())
      vs.pop_back();
    for (size_t i = 0; i < vs.size(); ++i)
      for (auto [x, y, s] : vertices(vs[(i + vs.size() - 1) % vs.size()], vs[i],
                                     vs[(i + 1) % vs.size()]))
        rows[y].emplace_back(x, s);
  }
  std::vector<Span> out;
  for (auto &[y, events] : rows) {
    std::sort(events.begin(), events.end());
    I winding = 0, left = 0;
    for (auto [x, sign] : events) {
      I old = winding;
      winding += sign;
      if (!old && winding)
        left = x;
      else if (old && !winding && x > left)
        out.push_back({left, x, y});
    }
    if (winding)
      throw std::runtime_error("unclosed glyph scan winding");
  }
  return out;
}
} // namespace NAMESPACE_PSAPI::TextBoundsDetail
