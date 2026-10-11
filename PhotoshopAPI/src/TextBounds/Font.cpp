#include "Font.h"
#include <limits>

namespace NAMESPACE_PSAPI::TextBoundsDetail {
Font::Font(std::string bytes, unsigned index) : data(std::move(bytes)) {
  blob = hb_blob_create(data.data(), static_cast<unsigned>(data.size()),
                        HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  face = hb_face_create(blob, index);
  hb = hb_font_create(face);
  hb_ot_font_set_funcs(hb);
  upem = static_cast<int>(hb_face_get_upem(face));
  hb_font_set_scale(hb, upem, upem);
  // Validate before acquiring FreeType handles, so a rejected font leaks none.
  if (table(HB_TAG('g', 'l', 'y', 'f')).empty() ||
      !table(HB_TAG('f', 'v', 'a', 'r')).empty()) {
    hb_font_destroy(hb);
    hb_face_destroy(face);
    hb_blob_destroy(blob);
    throw std::invalid_argument(
        "only non-variable TrueType outlines are supported");
  }
  auto head = table(HB_TAG('h', 'e', 'a', 'd'));
  head_flags = static_cast<int>(be16(head, 16));
  vmtx = table(HB_TAG('v', 'm', 't', 'x'));
  if (!vmtx.empty())
    vertical_count = be16(table(HB_TAG('v', 'h', 'e', 'a')), 34);
  if (FT_Init_FreeType(&library) ||
      FT_New_Memory_Face(library,
                         reinterpret_cast<const FT_Byte *>(data.data()),
                         static_cast<FT_Long>(data.size()), index, &ft)) {
    if (library)
      FT_Done_FreeType(library);
    hb_font_destroy(hb);
    hb_face_destroy(face);
    hb_blob_destroy(blob);
    throw std::invalid_argument("FreeType cannot load font");
  }
}
Font::~Font() {
  if (ft)
    FT_Done_Face(ft);
  if (library)
    FT_Done_FreeType(library);
  hb_font_destroy(hb);
  hb_face_destroy(face);
  hb_blob_destroy(blob);
}
std::vector<unsigned char> Font::table(hb_tag_t tag) const {
  auto b = hb_face_reference_table(face, tag);
  unsigned n = 0;
  const char *p = hb_blob_get_data(b, &n);
  std::vector<unsigned char> out(p, p + n);
  hb_blob_destroy(b);
  return out;
}
const Glyph &Font::outline(unsigned gid) {
  if (auto it = outlines.find(gid); it != outlines.end())
    return it->second;
  FT_Matrix identity = {65536, 0, 0, 65536};
  FT_Set_Transform(ft, &identity, nullptr);
  // At one em per design unit, 26.6 retains fractional composite coordinates.
  if (FT_Set_Char_Size(ft, static_cast<FT_F26Dot6>(upem * 64), 0, 72, 72) ||
      FT_Load_Glyph(ft, gid, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP))
    throw std::invalid_argument("cannot load glyph outline");
  const auto &o = ft->glyph->outline;
  Glyph g;
  for (int i = 0; i < o.n_points; ++i) {
    g.points.push_back({o.points[i].x / 64.0, o.points[i].y / 64.0});
    g.tags.push_back(static_cast<unsigned char>(o.tags[i]));
  }
  for (int i = 0; i < o.n_contours; ++i)
    g.ends.push_back(o.contours[i]);
  if (outlines.size() >= 256)
    outlines.clear();
  return outlines.emplace(gid, std::move(g)).first->second;
}
DPoint Font::vertical_origin(unsigned gid) {
  const auto &g = outline(gid);
  if (g.points.empty())
    return {0, 0};
  double ymax = -std::numeric_limits<double>::infinity();
  int start = 0;
  for (int end : g.ends) {
    std::vector<std::pair<DPoint, bool>> pts;
    for (int i = start; i <= end; ++i) {
      int j = i == end ? start : i + 1;
      bool on = g.tags[i] & 1;
      pts.emplace_back(g.points[i], on);
      if (!on && !(g.tags[j] & 1))
        pts.emplace_back(DPoint{(g.points[i][0] + g.points[j][0]) / 2,
                                (g.points[i][1] + g.points[j][1]) / 2},
                         true);
    }
    start = end + 1;
    auto first = std::find_if(pts.begin(), pts.end(),
                              [](const auto &p) { return p.second; });
    std::rotate(pts.begin(), first, pts.end());
    pts.push_back(pts.front());
    for (size_t i = 0; i + 1 < pts.size();) {
      double p = pts[i].first[1], c = pts[i + 1].first[1];
      ymax = std::max(ymax, p);
      if (pts[i + 1].second) {
        ymax = std::max(ymax, c);
        ++i;
      } else {
        double q = pts[i + 2].first[1];
        ymax = std::max(ymax, q);
        double den = p - 2 * c + q;
        if (den != 0) {
          double t = (p - c) / den;
          if (t > 0 && t < 1)
            ymax = std::max(ymax, (1 - t) * (1 - t) * p + 2 * (1 - t) * t * c +
                                      t * t * q);
        }
        i += 2;
      }
    }
  }
  size_t at = gid < vertical_count
                  ? gid * 4 + 2
                  : vertical_count * 4 + (gid - vertical_count) * 2;
  double top = signed16(vmtx, at) * 65536.0 / upem;
  I y = static_cast<I>(top + (top >= 0 ? 0.5 : -0.5)) +
        static_cast<I>(ymax * 65536 / upem);
  I width =
      static_cast<I>(hb_font_get_glyph_h_advance(hb, gid) * 65536.0 / upem);
  return {floor_div(width, 2) * upem / 65536.0, y * upem / 65536.0};
}
} // namespace NAMESPACE_PSAPI::TextBoundsDetail
