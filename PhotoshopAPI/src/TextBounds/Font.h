#pragma once
#include "Scan.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include <hb-ot.h>
#include <hb.h>
#include <map>
#include <stdexcept>
#include <string>

namespace NAMESPACE_PSAPI::TextBoundsDetail {
using DPoint = std::array<double, 2>;
struct Glyph {
  std::vector<DPoint> points;
  std::vector<unsigned char> tags;
  std::vector<int> ends;
};
struct Font {
  std::string data;
  FT_Library library = nullptr;
  FT_Face ft = nullptr;
  hb_blob_t *blob = nullptr;
  hb_face_t *face = nullptr;
  hb_font_t *hb = nullptr;
  int upem = 0, head_flags = 0;
  std::vector<unsigned char> vmtx;
  unsigned vertical_count = 0;
  std::map<unsigned, Glyph> outlines;
  Font(std::string bytes, unsigned index);
  ~Font();
  std::vector<unsigned char> table(hb_tag_t tag) const;
  unsigned glyph(char32_t c) const {
    hb_codepoint_t gid = 0;
    hb_font_get_nominal_glyph(hb, static_cast<uint32_t>(c), &gid);
    return gid;
  }
  const Glyph &outline(unsigned gid);
  DPoint vertical_origin(unsigned gid);
};
inline unsigned be16(const std::vector<unsigned char> &b, size_t at) {
  return (unsigned(b.at(at)) << 8) | b.at(at + 1);
}
inline int signed16(const std::vector<unsigned char> &b, size_t at) {
  unsigned n = be16(b, at);
  return n >= 32768 ? static_cast<int>(n) - 65536 : static_cast<int>(n);
}
} // namespace NAMESPACE_PSAPI::TextBoundsDetail
