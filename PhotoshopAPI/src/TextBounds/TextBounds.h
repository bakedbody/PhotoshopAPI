#pragma once
#include "Macros.h"
#include <array>
#include <memory>
#include <string>

PSAPI_NAMESPACE_BEGIN
// A reusable font face. Measurement never mutates a document or its caches.
class TextBoundsEngine {
public:
  TextBoundsEngine(std::string font_data, unsigned face_index = 0);
  ~TextBoundsEngine();
  TextBoundsEngine(const TextBoundsEngine &) = delete;
  TextBoundsEngine &operator=(const TextBoundsEngine &) = delete;
  std::array<int, 4> measure(const std::u32string &text, double size,
                             std::array<double, 2> position,
                             double leading = 1.2,
                             const std::string &kerning = "metrics",
                             const std::string &antialias = "smooth",
                             int baseline_direction = 1,
                             bool alternates = false, bool old_style = false,
                             const std::string &orientation = "vertical");
  double optical_adjustment(unsigned left, unsigned right, double size);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
PSAPI_NAMESPACE_END
