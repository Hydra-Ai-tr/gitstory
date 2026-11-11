#include "cli.hpp"

#include <charconv>
#include <cstdint>

namespace gitstory {

namespace {

std::int64_t parse_positive(const std::string& option, const std::string& text,
                            std::int64_t max) {
  std::int64_t value = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto res = std::from_chars(begin, end, value);
  if (text.empty() || res.ec != std::errc() || res.ptr != end || value < 1 || value > max) {
    throw UsageError(option + " expects a whole number between 1 and " + std::to_string(max) +
                     ", got '" + text + "'");
  }
  return value;
}

}  // namespace

std::string usage_text() {
  return "gitstory " GITSTORY_VERSION
         " - the story of a git repository, told in your terminal\n"
         "\n"
         "Usage: gitstory [path] [options]\n"
         "\n"
         "  path                  Repository to analyse (default: current directory)\n"
         "\n"
         "Selecting history:\n"
         "      --rev <rev>       Branch, tag or range to walk (default: HEAD)\n"
         "      --all             Walk every branch and tag\n"
         "      --since <date>    Only commits after this date   (e.g. 2024-01-01, \"1 year ago\")\n"
         "      --until <date>    Only commits before this date\n"
         "      --author <text>   Only commits whose author matches\n"
         "      --no-merges       Leave merge commits out\n"
         "  -n, --max-count <n>   Stop after the newest <n> commits\n"
         "\n"
         "Output:\n"
         "      --roast           Add a light-hearted roast of the repository\n"
         "      --json            Print every statistic as JSON\n"
         "      --svg <file>      Write a shareable SVG card ('-' for stdout)\n"
         "      --light           Use the light theme for the SVG card\n"
         "      --top <n>         Rows per ranking (default: 10)\n"
         "      --width <n>       Report width in columns (default: terminal width)\n"
         "      --color <when>    auto, always or never (default: auto; honours NO_COLOR)\n"
         "      --glyphs <set>    auto, full, basic (classic Windows console) or ascii\n"
         "      --ascii           Same as --glyphs ascii\n"
         "\n"
         "  -h, --help            Show this help\n"
         "  -V, --version         Show the version\n";
}

Options parse_args(const std::vector<std::string>& args) {
  Options options;
  bool have_path = false;
  bool only_positionals = false;

  for (std::size_t i = 0; i < args.size(); ++i) {
    std::string arg = args[i];

    if (only_positionals || arg.empty() || arg[0] != '-' || arg == "-") {
      if (have_path) throw UsageError("unexpected extra argument '" + arg + "'");
      if (arg.empty()) throw UsageError("the repository path must not be empty");
      options.log.repo = arg;
      have_path = true;
      continue;
    }
    if (arg == "--") {
      only_positionals = true;
      continue;
    }

    // Split "--name=value" so both spellings work.
    std::string inline_value;
    bool has_inline = false;
    if (arg.compare(0, 2, "--") == 0) {
      const std::size_t eq = arg.find('=');
      if (eq != std::string::npos) {
        inline_value = arg.substr(eq + 1);
        arg.erase(eq);
        has_inline = true;
      }
    }
    const auto value = [&]() -> std::string {
      if (has_inline) return inline_value;
      if (i + 1 >= args.size()) throw UsageError(arg + " needs a value");
      return args[++i];
    };
    const auto flag = [&] {
      if (has_inline) throw UsageError(arg + " does not take a value");
    };

    if (arg == "-h" || arg == "--help") {
      flag();
      options.help = true;
    } else if (arg == "-V" || arg == "--version") {
      flag();
      options.version = true;
    } else if (arg == "--json") {
      flag();
      options.json = true;
    } else if (arg == "--roast") {
      flag();
      options.roast = true;
    } else if (arg == "--ascii") {
      flag();
      options.glyphs = GlyphMode::Ascii;
    } else if (arg == "--glyphs") {
      const std::string set = value();
      if (set == "auto") {
        options.glyphs = GlyphMode::Auto;
      } else if (set == "full") {
        options.glyphs = GlyphMode::Full;
      } else if (set == "basic") {
        options.glyphs = GlyphMode::Basic;
      } else if (set == "ascii") {
        options.glyphs = GlyphMode::Ascii;
      } else {
        throw UsageError("--glyphs expects auto, full, basic or ascii, got '" + set + "'");
      }
    } else if (arg == "--light") {
      flag();
      options.light_theme = true;
    } else if (arg == "--all") {
      flag();
      options.log.all = true;
    } else if (arg == "--no-merges") {
      flag();
      options.log.no_merges = true;
    } else if (arg == "--rev") {
      options.log.rev = value();
      if (options.log.rev.empty()) throw UsageError("--rev needs a non-empty value");
    } else if (arg == "--since") {
      options.log.since = value();
    } else if (arg == "--until") {
      options.log.until = value();
    } else if (arg == "--author") {
      options.log.author = value();
    } else if (arg == "-n" || arg == "--max-count") {
      options.log.max_count = parse_positive(arg, value(), 1000000000);
    } else if (arg == "--top") {
      options.top = static_cast<std::size_t>(parse_positive(arg, value(), 1000));
    } else if (arg == "--width") {
      options.width = static_cast<std::size_t>(parse_positive(arg, value(), 1000));
    } else if (arg == "--svg") {
      options.svg_path = value();
      if (options.svg_path.empty()) throw UsageError("--svg needs a file name");
    } else if (arg == "--color") {
      const std::string when = value();
      if (when == "auto") {
        options.color = ColorMode::Auto;
      } else if (when == "always") {
        options.color = ColorMode::Always;
      } else if (when == "never") {
        options.color = ColorMode::Never;
      } else {
        throw UsageError("--color expects auto, always or never, got '" + when + "'");
      }
    } else {
      throw UsageError("unknown option '" + args[i] + "'");
    }
  }

  if (options.log.all && !options.log.rev.empty()) {
    throw UsageError("--all and --rev cannot be combined");
  }
  if (options.json && options.svg_path == "-") {
    throw UsageError("--json and --svg - would both write to standard output");
  }
  return options;
}

}  // namespace gitstory
