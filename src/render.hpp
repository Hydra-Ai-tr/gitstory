// The three output formats: terminal report, JSON document and SVG card.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "analyze.hpp"
#include "git.hpp"

namespace gitstory {

// Which characters the terminal's font can be trusted with.
enum class GlyphSet {
  Full,   // eighth blocks, rounded corners: modern terminals
  Basic,  // the WGL4 subset every Windows console font has (no partial eighths)
  Ascii,
};

struct TermOptions {
  bool color = false;  // ANSI colours
  GlyphSet glyphs = GlyphSet::Full;
  std::size_t width = 80;
  std::size_t top = 10;  // rows per ranking
  bool roast = false;
};

std::string render_terminal(const Stats& stats, const RepoInfo& repo, const TermOptions& options);

std::string render_json(const Stats& stats, const RepoInfo& repo);

struct SvgOptions {
  bool dark = true;
};

std::string render_svg(const Stats& stats, const RepoInfo& repo, const SvgOptions& options);

std::string json_escape(std::string_view text);
std::string xml_escape(std::string_view text);

}  // namespace gitstory
