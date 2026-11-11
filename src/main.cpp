#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include "analyze.hpp"
#include "cli.hpp"
#include "git.hpp"
#include "platform.hpp"
#include "render.hpp"

namespace {

constexpr int kExitFailure = 1;
constexpr int kExitUsage = 2;

void complain(const std::string& message) {
  std::fprintf(stderr, "gitstory: %s\n", message.c_str());
}

int run(const gitstory::Options& options, gitstory::Lang lang) {
  using namespace gitstory;

  const RepoInfo repo = describe_repo(options.log.repo);
  Analyzer analyzer(options.top);
  read_history(options.log, [&analyzer](Commit&& commit) { analyzer.add(commit); });
  const Stats stats = analyzer.finish();

  if (stats.commits == 0) {
    complain("no commits found in '" + repo.name +
             "' (the repository is empty or the filters matched nothing)");
    return kExitFailure;
  }

  ConsoleSession console;

  if (!options.svg_path.empty()) {
    SvgOptions svg;
    svg.dark = !options.light_theme;
    svg.lang = lang;
    const std::string card = render_svg(stats, repo, svg);
    if (options.svg_path == "-") {
      write_stdout(card);
    } else {
      std::string error;
      if (!write_file(options.svg_path, card, error)) {
        complain("cannot write '" + options.svg_path + "': " + error);
        return kExitFailure;
      }
      std::fprintf(stderr, "gitstory: wrote %s\n", options.svg_path.c_str());
    }
  }

  if (options.json) {
    write_stdout(render_json(stats, repo, lang));
  } else if (options.svg_path.empty()) {
    TermOptions term;
    term.lang = lang;
    switch (options.glyphs) {
      case GlyphMode::Full: term.glyphs = GlyphSet::Full; break;
      case GlyphMode::Basic: term.glyphs = GlyphSet::Basic; break;
      case GlyphMode::Ascii: term.glyphs = GlyphSet::Ascii; break;
      case GlyphMode::Auto:
        term.glyphs = console.full_glyphs() ? GlyphSet::Full : GlyphSet::Basic;
        break;
    }
    term.roast = options.roast;
    term.top = options.top;
    term.width = options.width != 0 ? options.width : terminal_width();
    if (term.width == 0) term.width = 80;
    switch (options.color) {
      case ColorMode::Always: term.color = true; break;
      case ColorMode::Never: term.color = false; break;
      case ColorMode::Auto:
        // https://no-color.org: any non-empty NO_COLOR disables colour.
        const char* no_color = std::getenv("NO_COLOR");
        term.color = console.supports_color() && (no_color == nullptr || no_color[0] == '\0');
        break;
    }
    write_stdout(render_terminal(stats, repo, term));
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  gitstory::Options options;
  try {
    options = gitstory::parse_args(gitstory::utf8_args(argc, argv));
  } catch (const gitstory::UsageError& error) {
    complain(error.what());
    std::fprintf(stderr, "Try 'gitstory --help' for more information.\n");
    return kExitUsage;
  }

  const gitstory::Lang lang = options.lang.value_or(gitstory::preferred_language());
  if (options.help) {
    gitstory::ConsoleSession console;
    gitstory::write_stdout(gitstory::usage_text(lang));
    return 0;
  }
  if (options.version) {
    std::puts("gitstory " GITSTORY_VERSION);
    return 0;
  }

  try {
    return run(options, lang);
  } catch (const gitstory::GitError& error) {
    complain(error.what());
    return kExitFailure;
  } catch (const std::exception& error) {
    complain(std::string("unexpected error: ") + error.what());
    return kExitFailure;
  }
}
