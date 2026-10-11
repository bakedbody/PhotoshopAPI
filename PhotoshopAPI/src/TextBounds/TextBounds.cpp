#include "TextBounds.h"
#include "Optical.h"
#include <limits>
#include <mutex>
#include <tuple>

PSAPI_NAMESPACE_BEGIN
using namespace TextBoundsDetail;
struct TextBoundsEngine::Impl {
  Font font;
  std::unique_ptr<Optical> optical;
  std::mutex mutex;
  std::map<std::tuple<unsigned, double, bool>, std::vector<Span>> spans;
  explicit Impl(std::string data, unsigned index)
      : font(std::move(data), index) {}
  const std::vector<Span> &glyph_spans(unsigned gid, double size,
                                       bool rotated) {
    auto key = std::make_tuple(gid, size, rotated);
    if (auto it = spans.find(key); it != spans.end())
      return it->second;
    double ppem = size * 4;
    if (font.head_flags & 8)
      ppem = std::floor(ppem + 0.5);
    // nearbyint preserves Python round's ties-to-even behavior.
    if (FT_Set_Char_Size(font.ft,
                         static_cast<FT_F26Dot6>(std::nearbyint(ppem * 64)), 0,
                         72, 72))
      throw std::invalid_argument("FreeType cannot load this font size");
    FT_Matrix matrix = rotated ? FT_Matrix{0, 65536, -65536, 0}
                               : FT_Matrix{65536, 0, 0, 65536};
    FT_Set_Transform(font.ft, &matrix, nullptr);
    if (FT_Load_Glyph(font.ft, gid, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP))
      throw std::invalid_argument("FreeType cannot load glyph");
    const auto &o = font.ft->glyph->outline;
    std::vector<Point> points;
    std::vector<unsigned char> tags;
    std::vector<int> ends;
    for (int i = 0; i < o.n_points; ++i) {
      if ((o.tags[i] & 3) == 2)
        throw std::invalid_argument("cubic outlines are not supported");
      points.push_back({o.points[i].x, o.points[i].y});
      tags.push_back(static_cast<unsigned char>(o.tags[i]));
    }
    for (int i = 0; i < o.n_contours; ++i)
      ends.push_back(o.contours[i]);
    auto result = scan(points, tags, ends);
    if (spans.size() >= 256)
      spans.clear();
    return spans.emplace(key, std::move(result)).first->second;
  }
  double adjust(unsigned left, unsigned right, double size) {
    if (!optical)
      optical = std::make_unique<Optical>(font);
    return optical->adjustment(left, right, size);
  }
};
TextBoundsEngine::TextBoundsEngine(std::string bytes, unsigned index)
    : m_impl(std::make_unique<Impl>(std::move(bytes), index)) {}
TextBoundsEngine::~TextBoundsEngine() = default;
double TextBoundsEngine::optical_adjustment(unsigned left, unsigned right,
                                            double size) {
  std::lock_guard guard(m_impl->mutex);
  return m_impl->adjust(left, right, size);
}
namespace {
bool space(char32_t c) {
  return c == 0x20 || (c >= 9 && c <= 13) || (c >= 0x1c && c <= 0x1f) ||
         c == 0x85 || c == 0xa0 || c == 0x1680 ||
         (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
         c == 0x202f || c == 0x205f || c == 0x3000;
}
} // namespace
std::array<int, 4>
TextBoundsEngine::measure(const std::u32string &input, double size,
                          std::array<double, 2> pos, double leading,
                          const std::string &mode, const std::string &aa,
                          int baseline, bool alternates, bool oldstyle,
                          const std::string &orientation) {
  if (!std::isfinite(size) || size <= 0 || !std::isfinite(leading) ||
      leading <= 0)
    throw std::invalid_argument(
        "font size and auto leading must be finite positive values");
  if (mode != "manual" && mode != "metrics" && mode != "optical" &&
      mode != "alternates" && mode != "oldstyle")
    throw std::invalid_argument("unsupported kerning mode");
  if (aa != "smooth" && aa != "crisp")
    throw std::invalid_argument("unsupported antialias mode");
  if (baseline != 1 && baseline != 2)
    throw std::invalid_argument("unsupported baseline direction");
  if (orientation != "vertical" && orientation != "horizontal")
    throw std::invalid_argument("unsupported writing direction");
  bool vertical = orientation == "vertical";
  std::u32string text;
  for (size_t i = 0; i < input.size(); ++i) {
    char32_t c = input[i];
    if (c == U'\t')
      throw std::invalid_argument("tab-stop layout is not implemented");
    if ((c >= 0x591 && c < 0x10a0) || (c >= 0x1781 && c < 0x1e00))
      throw std::invalid_argument(
          "complex-script run itemization still requires Photoshop");
    if (vertical && baseline == 2 && c >= 256 && !space(c) &&
        std::u32string(U"\u201c\u6d4b\u8bd5\u6587\u5b57\uff01\u201d").find(c) ==
            std::u32string::npos)
      throw std::invalid_argument(
          "automatic sideways-run layout is not implemented for this text");
    if (c == U'\r') {
      if (i + 1 < input.size() && input[i + 1] == U'\n')
        ++i;
      c = U'\n';
    }
    text.push_back(c);
  }
  std::lock_guard guard(m_impl->mutex);
  auto &font = m_impl->font;
  bool visible = false;
  std::array<I, 4> bounds = {
      std::numeric_limits<int>::max(), std::numeric_limits<int>::max(),
      std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
  size_t start = 0;
  int column = 0;
  while (start <= text.size()) {
    size_t stop = text.find(U'\n', start);
    if (stop == std::u32string::npos)
      stop = text.size();
    double along = 0;
    auto rotated_for = [&](char32_t c) {
      return vertical && baseline == 2 &&
             (c == 0x201c || c == 0x201d || c < 256);
    };
    for (size_t a = start; a < stop;) {
      bool rotated = rotated_for(text[a]);
      size_t b = a + 1;
      while (b < stop && rotated_for(text[b]) == rotated)
        ++b;
      std::vector<uint32_t> run;
      for (size_t i = a; i < b; ++i)
        run.push_back(static_cast<uint32_t>(text[i]));
      bool horizontal = !vertical || rotated,
           no_metrics = mode == "manual" || mode == "optical";
      auto *buffer = hb_buffer_create();
      hb_buffer_add_utf32(buffer, run.data(), static_cast<int>(run.size()), 0,
                          static_cast<int>(run.size()));
      hb_buffer_set_direction(buffer,
                              horizontal ? HB_DIRECTION_LTR : HB_DIRECTION_TTB);
      hb_buffer_set_script(buffer, HB_SCRIPT_HAN);
      hb_buffer_set_language(buffer, hb_language_from_string("zh-Hans", -1));
      std::vector<hb_feature_t> features;
      auto feature = [&](const char *name, bool value) {
        hb_feature_t f{};
        hb_feature_from_string(name, -1, &f);
        f.value = value;
        features.push_back(f);
      };
      feature("dlig", true);
      feature("salt", alternates || mode == "alternates");
      feature("onum", oldstyle || mode == "oldstyle");
      feature("pnum", oldstyle || mode == "oldstyle");
      if (rotated)
        feature("palt", !no_metrics);
      else if (vertical && baseline == 1)
        feature("vpal", !no_metrics);
      if (no_metrics) {
        feature("kern", false);
        feature("vkrn", false);
      }
      hb_shape(font.hb, buffer, features.data(),
               static_cast<unsigned>(features.size()));
      unsigned count = 0;
      auto infos = hb_buffer_get_glyph_infos(buffer, &count);
      auto positions = hb_buffer_get_glyph_positions(buffer, nullptr);
      // Own the buffer through exceptions from outline and optical processing.
      std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> owner(
          buffer, hb_buffer_destroy);
      unsigned previous = 0;
      for (unsigned i = 0; i < count; ++i) {
        unsigned gid = infos[i].codepoint;
        if (!gid)
          throw std::invalid_argument("font is missing a required glyph");
        const auto &p = positions[i];
        if (mode == "optical" && horizontal && previous)
          along += m_impl->adjust(previous, gid, size) * font.upem / size;
        double x, y, advance;
        if (!vertical) {
          x = along + p.x_offset;
          y = -p.y_offset;
          advance = p.x_advance;
        } else if (rotated) {
          hb_position_t ox, oy;
          hb_font_get_glyph_v_origin(font.hb, gid, &ox, &oy);
          x = font.upem / 2.0 - oy + p.y_offset;
          y = along + p.x_offset;
          advance = p.x_advance;
        } else {
          x = p.x_offset;
          y = along - p.y_offset;
          advance = -p.y_advance;
          if (font.vmtx.empty()) {
            double extra =
                font.upem + hb_font_get_glyph_v_advance(font.hb, gid);
            y += extra / 2;
            advance += extra;
          } else {
            hb_position_t ox, oy;
            hb_font_get_glyph_v_origin(font.hb, gid, &ox, &oy);
            auto origin = font.vertical_origin(gid);
            x += ox - origin[0];
            y += origin[1] - oy;
          }
          if (space(static_cast<char32_t>(run[infos[i].cluster]))) {
            advance = hb_font_get_glyph_h_advance(font.hb, gid);
            advance = static_cast<I>(advance * 65536 / font.upem) * font.upem /
                      65536.0;
          }
        }
        double origin_x = pos[0] - (vertical ? column * size * leading : 0) +
                          x * size / font.upem;
        double origin_y = pos[1] + (!vertical ? column * size * leading : 0) +
                          y * size / font.upem;
        I x4 = static_cast<I>(std::floor(
            (aa == "smooth" || rotated) ? origin_x * 4 + 0.5 : origin_x + 0.5));
        if (aa != "smooth" && !rotated)
          x4 *= 4;
        I y4 = static_cast<I>(
            std::floor(aa == "smooth" ? origin_y * 4 + 0.5 : origin_y + 0.5));
        if (aa != "smooth")
          y4 *= 4;
        // Both AA transfer curves map every nonzero sample count to
        // nonzero alpha. Bounds need spans, not a full bitmap allocation.
        for (auto [left, right, row] :
             m_impl->glyph_spans(gid, size, rotated)) {
          visible = true;
          bounds[0] = std::min(bounds[0], floor_div(left + x4, 4));
          bounds[2] = std::max(bounds[2], floor_div(right + x4 + 3, 4));
          bounds[1] = std::min(bounds[1], floor_div(-row - 1 + y4, 4));
          bounds[3] = std::max(bounds[3], floor_div(-row + y4 + 3, 4));
        }
        along += advance;
        previous = gid;
      }
      a = b;
    }
    if (stop == text.size())
      break;
    start = stop + 1;
    ++column;
  }
  if (!visible)
    throw std::invalid_argument("text has no visible pixels");
  return {static_cast<int>(bounds[0]), static_cast<int>(bounds[1]),
          static_cast<int>(bounds[2]), static_cast<int>(bounds[3])};
}
PSAPI_NAMESPACE_END
