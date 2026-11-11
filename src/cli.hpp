// Command-line parsing.
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "git.hpp"

namespace gitstory {

enum class ColorMode { Auto, Always, Never };
enum class GlyphMode { Auto, Full, Basic, Ascii };

struct Options {
  LogOptions log;
  bool help = false;
  bool version = false;
  bool json = false;
  bool roast = false;
  GlyphMode glyphs = GlyphMode::Auto;
  bool light_theme = false;
  std::string svg_path;  // empty = no card, "-" = stdout
  ColorMode color = ColorMode::Auto;
  std::size_t top = 10;
  std::size_t width = 0;  // 0 = detect
};

class UsageError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// `args` excludes the program name. Throws UsageError on bad input.
Options parse_args(const std::vector<std::string>& args);

std::string usage_text();

}  // namespace gitstory
