#include "render.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "util.hpp"

namespace gitstory {

namespace {

constexpr std::int64_t kDay = 86400;
constexpr const char* kProjectUrl = "github.com/Hydra-Ai-tr/gitstory";

// ANSI styles (xterm-256). Everything goes through Painter so `--color never` is exact.
constexpr const char* kTitle = "1;38;5;231";
constexpr const char* kStrong = "1;38;5;255";
constexpr const char* kAccent = "38;5;117";
constexpr const char* kLabel = "38;5;245";
constexpr const char* kFaint = "38;5;240";
constexpr const char* kGreen = "38;5;114";
constexpr const char* kRed = "38;5;210";
constexpr const char* kYellow = "38;5;222";
constexpr const char* kHighlight = "1;38;5;222";
constexpr const char* kBar = "38;5;74";
constexpr const char* kPeak = "38;5;215";
constexpr const char* kHeat[] = {"38;5;238", "38;5;22", "38;5;28", "38;5;35", "38;5;83"};

constexpr const char* kWeekdayLong[] = {"Monday", "Tuesday", "Wednesday", "Thursday",
                                        "Friday", "Saturday", "Sunday"};

struct Glyphs {
  const char* rule;
  const char* side;
  const char* top_left;
  const char* top_right;
  const char* bottom_left;
  const char* bottom_right;
  const char* ellipsis;
  const char* dot;
  const char* arrow;
  const char* bullet;
  const char* marker;
  const char* cell;          // heat-map square, coloured by level
  const char* full;          // a completely filled bar cell
  const char* const* hpart;  // partial horizontal cells, index = sub-steps
  int hsteps;
  const char* const* vpart;  // partial vertical cells, index = sub-steps (0 = blank)
  int vsteps;
  const char* const* shades;  // five heat levels when there is no colour
  const char* const* fills;   // language segments when there is no colour
};

constexpr const char* kFullH[] = {"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
constexpr const char* kFullV[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇"};
constexpr const char* kBasicH[] = {"", "▌"};
constexpr const char* kBasicV[] = {" ", "▄"};
constexpr const char* kAsciiH[] = {""};
constexpr const char* kAsciiV[] = {" ", "."};
constexpr const char* kShades[] = {"·", "░", "▒", "▓", "█"};
constexpr const char* kAsciiShades[] = {".", "-", "+", "*", "#"};
constexpr const char* kFills[] = {"█", "▓", "▒", "░"};
constexpr const char* kAsciiFills[] = {"#", "=", "+", "-"};

const Glyphs& glyphs_for(GlyphSet set) {
  static const Glyphs full = {"─", "│", "╭", "╮", "╰", "╯", "…", " · ", " → ", "• ", "●",
                              "■", "█", kFullH, 8, kFullV, 8, kShades, kFills};
  // Only characters from the WGL4 set, which the classic Windows console fonts cover.
  static const Glyphs basic = {"─", "│", "┌", "┐", "└", "┘", "…", " · ", " → ", "• ", "●",
                               "■", "█", kBasicH, 2, kBasicV, 2, kShades, kFills};
  static const Glyphs ascii = {"-", "|", "+", "+", "+", "+", "~", " | ", " -> ", "* ", "*",
                               "#", "#", kAsciiH, 1, kAsciiV, 2, kAsciiShades, kAsciiFills};
  switch (set) {
    case GlyphSet::Basic: return basic;
    case GlyphSet::Ascii: return ascii;
    case GlyphSet::Full: break;
  }
  return full;
}

struct Painter {
  bool on = false;
  std::string operator()(std::string_view style, std::string_view text) const {
    if (!on || text.empty() || style.empty()) return std::string(text);
    std::string out = "\x1b[";
    out += style;
    out += 'm';
    out += text;
    out += "\x1b[0m";
    return out;
  }
};

std::string repeat(std::string_view unit, std::size_t times) {
  std::string out;
  out.reserve(unit.size() * times);
  for (std::size_t i = 0; i < times; ++i) out += unit;
  return out;
}

std::string spaces(std::size_t n) { return std::string(n, ' '); }

std::string rtrim(std::string s) {
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}

// 12345 -> "12.3k": tiles have room for only a few glyphs.
std::string compact(std::int64_t n) {
  if (n < 10000) return format_number(n);
  char buf[32];
  if (n < 1000000) {
    std::snprintf(buf, sizeof buf, "%.1fk", static_cast<double>(n) / 1000.0);
  } else {
    std::snprintf(buf, sizeof buf, "%.1fM", static_cast<double>(n) / 1000000.0);
  }
  std::string out = buf;
  for (char& c : out) {
    if (c == ',') c = '.';
  }
  return out;
}

std::string percent1(double ratio) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.1f%%", ratio * 100.0);
  std::string out = buf;
  for (char& c : out) {
    if (c == ',') c = '.';
  }
  return out;
}

std::string count_of(std::int64_t n, const char* singular, const char* many) {
  return format_number(n) + " " + (n == 1 ? singular : many);
}

// "8 days", "5 months", "3 years and 2 months": for sentences, not tables.
std::string prose_span(std::int64_t days) {
  if (days <= 1) return "1 day";
  if (days < 60) return std::to_string(days) + " days";
  const auto months = static_cast<std::int64_t>(static_cast<double>(days) / 30.4375);
  if (months < 24) return std::to_string(months) + " months";
  const std::int64_t years = months / 12;
  const std::int64_t rest = months % 12;
  std::string out = std::to_string(years) + " years";
  if (rest != 0) out += " and " + count_of(rest, "month", "months");
  return out;
}

struct Piece {
  std::string text;
  std::size_t width = 0;
};

// Greedy line filling of pre-styled pieces; widths are the visible widths, so
// the (possibly styled) separator's visible width is passed in as well.
std::vector<std::string> flow(const std::vector<Piece>& pieces, std::string_view separator,
                              std::size_t sep_width, std::size_t width) {
  std::vector<std::string> lines;
  std::string current;
  std::size_t used = 0;
  for (const Piece& piece : pieces) {
    if (used > 0 && used + sep_width + piece.width > width) {
      lines.push_back(current);
      current.clear();
      used = 0;
    }
    if (used > 0) {
      current += separator;
      used += sep_width;
    }
    current += piece.text;
    used += piece.width;
  }
  if (used > 0) lines.push_back(current);
  return lines;
}

// Builds a paragraph from styled fragments. Text that does not start with a
// space sticks to the previous fragment, so "41 commits" + "." stays together.
class Prose {
 public:
  explicit Prose(const Painter& paint) : paint_(paint) {}

  Prose& add(std::string_view text, std::string_view style = {}) {
    std::size_t i = 0;
    while (i < text.size()) {
      if (text[i] == ' ') {
        space_ = true;
        ++i;
        continue;
      }
      std::size_t j = text.find(' ', i);
      if (j == std::string_view::npos) j = text.size();
      const std::string_view word = text.substr(i, j - i);
      std::string styled = paint_(style, word);
      if (!space_ && !pieces_.empty()) {
        pieces_.back().text += styled;
        pieces_.back().width += display_width(word);
      } else {
        pieces_.push_back({std::move(styled), display_width(word)});
      }
      space_ = false;
      i = j;
    }
    return *this;
  }

  std::vector<std::string> wrap(std::size_t width) const { return flow(pieces_, " ", 1, width); }

 private:
  const Painter& paint_;
  std::vector<Piece> pieces_;
  bool space_ = true;
};

// Places ASCII labels at column offsets, dropping any that would collide.
std::string place_labels(const std::vector<std::pair<std::size_t, std::string>>& labels,
                         std::size_t width) {
  std::string out(width, ' ');
  std::size_t free_from = 0;
  for (const auto& [pos, text] : labels) {
    if (pos < free_from || pos + text.size() > width) continue;
    out.replace(pos, text.size(), text);
    free_from = pos + text.size() + 1;
  }
  return rtrim(out);
}

class Report {
 public:
  Report(const Stats& stats, const RepoInfo& repo, const TermOptions& options)
      : s_(stats), repo_(repo), o_(options), g_(glyphs_for(options.glyphs)) {
    paint_.on = options.color;
    width_ = std::clamp<std::size_t>(options.width, 60, 110);
    inner_ = width_ - 4;
    top_ = std::max<std::size_t>(options.top, 1);
    const Civil first = civil_from_days(s_.first_day);
    const Civil last = civil_from_days(s_.last_day);
    one_year_ = first.year == last.year;
  }

  std::string build() {
    header();
    story();
    overview();
    activity();
    rhythm();
    highlights();
    authors();
    languages();
    files();
    words();
    messages();
    if (o_.roast) roast();
    footer();
    return std::move(out_);
  }

 private:
  // ---- building blocks ----

  void line(const std::string& text = {}) {
    if (!text.empty()) out_ += "  " + text;
    out_ += '\n';
  }

  void section(const char* title) {
    out_ += '\n';
    const std::string label = std::string(" ") + title + " ";
    const std::size_t used = 2 + display_width(label);
    const std::size_t rest = inner_ > used ? inner_ - used : 0;
    line(paint_(kFaint, repeat(g_.rule, 2)) + paint_(kTitle, label) +
         paint_(kFaint, repeat(g_.rule, rest)));
  }

  void field(const char* key, const std::string& value) {
    line(paint_(kLabel, pad_right(key, 16)) + value);
  }

  // Joins pieces with `separator`, wrapping to the report width.
  void flow_lines(const std::vector<Piece>& pieces, std::string_view separator) {
    for (const std::string& text :
         flow(pieces, paint_(kFaint, separator), display_width(separator), inner_)) {
      line(text);
    }
  }

  Piece labelled(const std::string& label, const std::string& value,
                 const char* style = kStrong) const {
    return {paint_(kLabel, label + " ") + paint_(style, value),
            display_width(label) + 1 + display_width(value)};
  }

  std::string hbar(std::int64_t value, std::int64_t max, std::size_t cells) const {
    if (max <= 0 || value <= 0 || cells == 0) return {};
    const auto steps = static_cast<std::size_t>(g_.hsteps);
    const double exact = static_cast<double>(value) / static_cast<double>(max) *
                         static_cast<double>(cells * steps);
    const std::size_t units = std::clamp<std::size_t>(static_cast<std::size_t>(exact + 0.5), 1,
                                                      cells * steps);
    std::string out = repeat(g_.full, units / steps);
    out += g_.hpart[units % steps];
    return out;
  }

  // A bar padded to `cells` columns.
  std::string hbar_cell(std::int64_t value, std::int64_t max, std::size_t cells,
                        const char* style = kBar) const {
    const std::string b = hbar(value, max, cells);
    return paint_(style, b) + spaces(cells - display_width(b));
  }

  std::string heat(std::int64_t value, std::int64_t max) const {
    int level = 0;
    if (value > 0 && max > 0) level = static_cast<int>((value * 4 - 1) / max) + 1;  // 1..4
    if (paint_.on) return paint_(kHeat[level], g_.cell) + " ";
    return std::string(g_.shades[level]) + " ";
  }

  // Vertical bars, one per value, each `bar` wide and followed by `gap` blanks.
  // With `values_on_top` each count sits just above its bar when it fits.
  void columns(const std::vector<std::int64_t>& values, std::size_t bar, std::size_t gap,
               std::size_t height, bool values_on_top, const std::string& top_label,
               std::size_t gutter) {
    if (values.empty()) return;
    const std::int64_t max = *std::max_element(values.begin(), values.end());
    const auto steps = static_cast<std::int64_t>(g_.vsteps);
    const auto total = static_cast<std::int64_t>(height) * steps;
    std::vector<std::int64_t> units(values.size(), 0);
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (values[i] <= 0 || max <= 0) continue;
      const double exact =
          static_cast<double>(values[i]) / static_cast<double>(max) * static_cast<double>(total);
      units[i] = std::clamp<std::int64_t>(static_cast<std::int64_t>(exact + 0.5), 1, total);
    }
    const std::size_t rows = height + (values_on_top ? 1 : 0);
    for (std::size_t r = 0; r < rows; ++r) {
      const auto level = static_cast<std::int64_t>(rows - 1 - r);  // 0 = bottom row
      std::string row = r == 0 && !top_label.empty()
                            ? paint_(kLabel, pad_left(top_label, gutter - 1)) + " "
                            : spaces(gutter);
      for (std::size_t i = 0; i < values.size(); ++i) {
        const std::int64_t filled_rows = (units[i] + steps - 1) / steps;
        if (values_on_top && values[i] > 0 && level == filled_rows) {
          const std::string label = format_number(values[i]);
          if (label.size() <= bar) {
            const std::size_t left = (bar - label.size()) / 2;
            row += spaces(left) + paint_(kLabel, label) + spaces(bar - label.size() - left);
          } else {
            row += spaces(bar);
          }
        } else if (level < static_cast<std::int64_t>(height)) {
          const std::int64_t u = std::clamp<std::int64_t>(units[i] - level * steps, 0, steps);
          if (u == 0) {
            row += spaces(bar);
          } else {
            const char* glyph = u == steps ? g_.full : g_.vpart[u];
            row += paint_(values[i] == max ? kPeak : kBar, repeat(glyph, bar));
          }
        } else {
          row += spaces(bar);
        }
        if (i + 1 < values.size()) row += spaces(gap);
      }
      line(rtrim(row));
    }
  }

  std::string short_date(std::int64_t day) const {
    const Civil c = civil_from_days(day);
    std::string out = std::string(month_name(c.month)) + " " + std::to_string(c.day);
    if (!one_year_) out += ", " + std::to_string(c.year);
    return out;
  }

  std::string lines_changed(std::int64_t added, std::int64_t deleted) const {
    return paint_(kGreen, "+" + format_number(added)) + " " +
           paint_(kRed, "-" + format_number(deleted));
  }

  std::string quoted(const std::string& subject, std::size_t width) const {
    const std::string text = subject.empty() ? "(no message)" : subject;
    return "\"" + truncate_right(text, width, g_.ellipsis) + "\"";
  }

  static std::string short_hash(const std::string& hash) { return hash.substr(0, 7); }

  static double share(std::int64_t part, std::int64_t whole) {
    return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0.0;
  }

  static std::string display_name(const AuthorStat& a) {
    if (!a.name.empty()) return a.name;
    return a.email.empty() ? "(unknown)" : a.email;
  }

  int peak_hour() const {
    return static_cast<int>(std::max_element(s_.by_hour.begin(), s_.by_hour.end()) -
                            s_.by_hour.begin());
  }

  int peak_weekday() const {
    return static_cast<int>(std::max_element(s_.by_weekday.begin(), s_.by_weekday.end()) -
                            s_.by_weekday.begin());
  }

  static std::string hour_text(int hour) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:00", hour);
    return buf;
  }

  // ---- sections ----

  void header() {
    out_ += '\n';
    const std::size_t box = inner_;
    const std::size_t room = box - 4;  // "│ " ... " │"
    const auto row = [&](const std::string& styled, std::size_t visible) {
      line(paint_(kFaint, g_.side) + " " + styled + spaces(room > visible ? room - visible : 0) +
           " " + paint_(kFaint, g_.side));
    };
    line(paint_(kFaint, std::string(g_.top_left) + repeat(g_.rule, box - 2) + g_.top_right));

    const std::string brand = "gitstory";
    const std::string name = truncate_right(repo_.name, room - brand.size() - 2, g_.ellipsis);
    const std::size_t name_width = display_width(name);
    row(paint_(kTitle, name) + spaces(room - name_width - brand.size()) + paint_(kAccent, brand),
        room);

    std::string span = format_date(s_.first_day);
    if (s_.last_day != s_.first_day) span += g_.arrow + format_date(s_.last_day);
    span += g_.dot + format_span_days(s_.span_days());
    span = truncate_right(span, room, g_.ellipsis);
    row(paint_(kLabel, span), display_width(span));
    line(paint_(kFaint, std::string(g_.bottom_left) + repeat(g_.rule, box - 2) + g_.bottom_right));
  }

  void story() {
    Prose p(paint_);
    const std::int64_t days = s_.span_days();
    p.add(repo_.name, kStrong);
    if (days <= 1) {
      p.add(" came together in a single day, " + format_date(s_.first_day) + ", in ");
    } else {
      p.add(" has been in the works for ");
      p.add(prose_span(days), kStrong);
      p.add(" (" + format_date(s_.first_day) + " to " + format_date(s_.last_day) + "), with ");
    }
    p.add(count_of(s_.commits, "commit", "commits"), kStrong);
    p.add(" by ");
    if (s_.author_count == 1) {
      p.add(display_name(s_.authors.front()), kStrong);
    } else if (s_.author_count == 2) {
      p.add(display_name(s_.authors[0]), kStrong);
      p.add(" and ");
      p.add(display_name(s_.authors[1]), kStrong);
    } else {
      p.add(format_number(s_.author_count) + " people", kStrong);
    }
    p.add(".");
    if (days > 1) {
      p.add(" Something landed on ");
      p.add(format_number(s_.active_days) + " of those " + format_number(days) + " days", kStrong);
      p.add(".");
    }

    p.add(" ");
    p.add(kWeekdayLong[peak_weekday()], kStrong);
    p.add(" is the busiest day of the week and ");
    p.add(hour_text(peak_hour()), kStrong);
    p.add(" the busiest hour");
    const double night = share(s_.night_commits, s_.commits);
    const double weekend = share(s_.weekend_commits, s_.commits);
    if (s_.night_commits > 0 && night >= 0.10) {
      p.add("; ");
      p.add(format_percent(night), kYellow);
      p.add(" of commits land after midnight");
    } else if (weekend >= 0.30) {
      p.add("; ");
      p.add(format_percent(weekend), kYellow);
      p.add(" of commits land on weekends");
    }
    p.add(".");

    const std::int64_t net = s_.added - s_.deleted;
    if (s_.added + s_.deleted > 0) {
      p.add(net >= 0 ? " The code grew by " : " The code shrank by ");
      p.add(count_of(net >= 0 ? net : -net, "line", "lines"), net >= 0 ? kGreen : kRed);
      if (!s_.top_files.empty()) {
        p.add(", and ");
        p.add(s_.top_files.front().path, kAccent);
        p.add(" changed more often than any other file");
      }
      p.add(".");
    }

    // Only real languages make it into the sentence, not images or lockfiles.
    std::vector<const LanguageStat*> code;
    std::int64_t total = 0;
    for (const LanguageStat& l : s_.languages) {
      total += l.lines;
      if (l.name != "Other" && l.name != "Lockfiles" && l.name != "Images" && l.name != "Text") {
        code.push_back(&l);
      }
    }
    if (!code.empty() && share(code.front()->lines, total) >= 0.25) {
      p.add(" It is mostly ");
      p.add(code.front()->name, kStrong);
      if (code.size() > 1 && share(code[1]->lines, total) >= 0.10) {
        p.add(" with a good helping of ");
        p.add(code[1]->name, kStrong);
      }
      p.add(".");
    }

    if (s_.author_count > 1) {
      const AuthorStat& lead = s_.authors.front();
      p.add(" ");
      p.add(display_name(lead), kStrong);
      p.add(" leads with ");
      p.add(format_percent(share(lead.commits, s_.commits)), kStrong);
      p.add(" of the commits.");
    }

    line();
    for (const std::string& text : p.wrap(inner_)) line(text);
  }

  void overview() {
    struct Tile {
      std::string value;
      std::size_t width;
      std::string label;
    };
    std::vector<Tile> tiles;
    const auto add = [&](const std::string& plain, const std::string& label) {
      tiles.push_back({paint_(kStrong, plain), display_width(plain), label});
    };
    add(format_number(s_.commits), s_.commits == 1 ? "commit" : "commits");
    add(format_number(s_.author_count), s_.author_count == 1 ? "author" : "authors");
    add(format_number(s_.active_days) + "/" + format_number(s_.span_days()), "active days");
    const std::string plus = "+" + compact(s_.added);
    const std::string minus = "-" + compact(s_.deleted);
    tiles.push_back({paint_(kGreen, plus) + " " + paint_(kRed, minus),
                     plus.size() + 1 + minus.size(), "lines changed"});
    add(format_number(s_.files_touched), s_.files_touched == 1 ? "file" : "files");
    if (s_.merges > 0) add(format_number(s_.merges), s_.merges == 1 ? "merge" : "merges");

    std::size_t needed = 0;
    for (const Tile& t : tiles) needed = std::max({needed, t.width + 2, t.label.size() + 2});
    std::size_t per_row = tiles.size();
    while (per_row > 1 && inner_ / per_row < needed) --per_row;
    const std::size_t cell = inner_ / per_row;

    for (std::size_t start = 0; start < tiles.size(); start += per_row) {
      const std::size_t end = std::min(tiles.size(), start + per_row);
      std::string values;
      std::string labels;
      for (std::size_t i = start; i < end; ++i) {
        values += tiles[i].value + spaces(cell - tiles[i].width);
        labels += paint_(kLabel, tiles[i].label) + spaces(cell - tiles[i].label.size());
      }
      line();
      line(rtrim(values));
      line(rtrim(labels));
    }
  }

  void activity() {
    section("Activity");
    const std::int64_t days = s_.span_days();
    constexpr std::size_t gutter = 6;
    const std::size_t avail = inner_ - gutter;
    if (days <= 1) {
      line(paint_(kLabel, "Every commit happened on ") + format_date(s_.first_day) +
           paint_(kLabel, ". See the rhythm below for how the day went."));
      return;
    }
    const auto weeks = static_cast<std::size_t>(
        ((s_.last_day - weekday_from_days(s_.last_day)) -
         (s_.first_day - weekday_from_days(s_.first_day))) / 7 + 1);
    if (static_cast<std::size_t>(days) * 2 <= avail) {
      daily_chart(avail, gutter);
    } else if (weeks * 2 <= avail) {
      calendar(gutter);
    } else {
      long_chart(avail, gutter);
    }
    line();
    const std::string on_days = count_of(s_.commits, "commit", "commits") + " on " +
                                format_number(s_.active_days) + " of " + format_number(days) +
                                " days";
    flow_lines({{paint_(kLabel, on_days), display_width(on_days)},
                labelled("busiest", short_date(s_.busiest_day) + " (" +
                                        format_number(s_.busiest_day_commits) + ")")},
               g_.dot);
  }

  void daily_chart(std::size_t avail, std::size_t gutter) {
    const auto days = static_cast<std::size_t>(s_.span_days());
    std::vector<std::int64_t> values(days, 0);
    for (const auto& [day, count] : s_.daily) {
      values[static_cast<std::size_t>(day - s_.first_day)] = count;
    }
    const std::size_t step = std::clamp<std::size_t>(avail / days, 2, 6);
    const std::size_t bar = step - 1;
    const bool on_top = bar >= 2;
    const std::int64_t max = *std::max_element(values.begin(), values.end());
    columns(values, bar, 1, 6, on_top, on_top ? std::string() : compact(max), gutter);
    const std::size_t chart = days * step - 1;
    line(spaces(gutter) + paint_(kFaint, repeat(g_.rule, chart)));

    std::vector<std::pair<std::size_t, std::string>> numbers;
    std::vector<std::pair<std::size_t, std::string>> weekdays;
    std::vector<std::pair<std::size_t, std::string>> months;
    for (std::size_t i = 0; i < days; ++i) {
      const auto day = s_.first_day + static_cast<std::int64_t>(i);
      const Civil c = civil_from_days(day);
      const std::size_t pos = i * step;
      if (bar >= 2) {
        const std::string number = std::to_string(c.day);
        numbers.push_back({pos + (bar - std::min(bar, number.size())) / 2, number});
        if (bar >= 3) {
          const std::string name = weekday_name(weekday_from_days(day));
          weekdays.push_back({pos + (bar - std::min(bar, name.size())) / 2, name});
        }
        if (i == 0 || c.day == 1) months.push_back({pos, month_name(c.month)});
      } else if (i % ((7 + step - 1) / step) == 0) {
        numbers.push_back({pos, short_date(day)});
      }
    }
    line(paint_(kLabel, spaces(gutter) + place_labels(numbers, chart)));
    if (!weekdays.empty()) line(paint_(kFaint, spaces(gutter) + place_labels(weekdays, chart)));
    if (!months.empty()) line(paint_(kLabel, spaces(gutter) + place_labels(months, chart)));
  }

  void calendar(std::size_t gutter) {
    const std::int64_t start = s_.first_day - weekday_from_days(s_.first_day);
    const std::int64_t end = s_.last_day - weekday_from_days(s_.last_day);
    const auto weeks = static_cast<std::size_t>((end - start) / 7 + 1);
    std::int64_t max = 0;
    for (const auto& [day, count] : s_.daily) max = std::max(max, count);

    std::vector<std::pair<std::size_t, std::string>> months;
    for (std::size_t w = 0; w < weeks; ++w) {
      for (std::int64_t d = 0; d < 7; ++d) {
        const std::int64_t day = start + static_cast<std::int64_t>(w) * 7 + d;
        if (day < s_.first_day || day > s_.last_day) continue;
        const Civil c = civil_from_days(day);
        if (c.day == 1 || day == s_.first_day) {
          months.push_back({w * 2, month_name(c.month)});
          break;
        }
      }
    }
    line(paint_(kLabel, spaces(gutter) + place_labels(months, weeks * 2)));
    for (int wd = 0; wd < 7; ++wd) {
      const bool labelled = wd % 2 == 0 && wd < 6;
      std::string row = paint_(kLabel, pad_right(labelled ? weekday_name(wd) : "", gutter));
      for (std::size_t w = 0; w < weeks; ++w) {
        const std::int64_t day = start + static_cast<std::int64_t>(w) * 7 + wd;
        if (day < s_.first_day || day > s_.last_day) {
          row += "  ";
          continue;
        }
        const auto it = s_.daily.find(day);
        row += heat(it == s_.daily.end() ? 0 : it->second, max);
      }
      line(rtrim(row));
    }
    std::string legend = spaces(gutter) + paint_(kLabel, "less ");
    for (int level = 0; level <= 4; ++level) legend += heat(level == 0 ? 0 : level, 4);
    line(legend + paint_(kLabel, "more"));
  }

  void long_chart(std::size_t avail, std::size_t gutter) {
    const Timeline t = bucket_timeline(s_, avail);
    if (t.buckets.empty()) return;
    const std::int64_t max = *std::max_element(t.buckets.begin(), t.buckets.end());
    // Square-root heights: one release-day spike would otherwise flatten years
    // of ordinary activity into a single row.
    std::vector<std::int64_t> heights;
    heights.reserve(t.buckets.size());
    for (const std::int64_t v : t.buckets) {
      heights.push_back(static_cast<std::int64_t>(std::sqrt(static_cast<double>(v)) * 1000.0));
    }
    columns(heights, 1, 0, 6, false, compact(max), gutter);
    line(spaces(gutter) + paint_(kFaint, repeat(g_.rule, t.buckets.size())));

    const bool by_year = s_.span_days() > 3 * 365;
    std::vector<std::pair<std::size_t, std::string>> labels;
    int previous = -1;
    for (std::size_t i = 0; i < t.buckets.size(); ++i) {
      const Civil c = civil_from_days(s_.first_day +
                                      static_cast<std::int64_t>(i) * t.days_per_bucket);
      const int key = by_year ? c.year : c.year * 12 + static_cast<int>(c.month);
      if (key == previous) continue;
      previous = key;
      if (by_year) {
        labels.push_back({i, std::to_string(c.year)});
      } else {
        std::string label = month_name(c.month);
        if (c.month == 1 || i == 0) label += " " + std::to_string(c.year);
        labels.push_back({i, label});
      }
    }
    line(paint_(kLabel, spaces(gutter) + place_labels(labels, t.buckets.size())));
    line(paint_(kFaint, spaces(gutter) + "each column = " +
                            count_of(t.days_per_bucket, "day", "days") + ", square-root scale"));
  }

  void rhythm() {
    section("Rhythm");
    constexpr std::size_t gutter = 5;
    const std::vector<std::int64_t> hours(s_.by_hour.begin(), s_.by_hour.end());
    columns(hours, 1, 1, 4, false, {}, gutter);

    std::int64_t cell_max = 0;
    for (const auto& row : s_.heatmap) {
      cell_max = std::max(cell_max, *std::max_element(row.begin(), row.end()));
    }
    const std::int64_t day_max = *std::max_element(s_.by_weekday.begin(), s_.by_weekday.end());
    // The grid is fixed at 48 columns; totals and bars only appear when they fit.
    const std::size_t grid = gutter + 48;
    const bool show_totals = grid + 6 <= inner_;
    const std::size_t bar_cells =
        show_totals && inner_ >= grid + 6 + 2 + 4 ? std::min<std::size_t>(10, inner_ - grid - 8) : 0;
    for (int wd = 0; wd < 7; ++wd) {
      const auto w = static_cast<std::size_t>(wd);
      std::string row = paint_(kLabel, pad_right(weekday_name(wd), gutter));
      for (int h = 0; h < 24; ++h) row += heat(s_.heatmap[w][static_cast<std::size_t>(h)], cell_max);
      if (show_totals) row += " " + pad_left(format_number(s_.by_weekday[w]), 5);
      if (bar_cells > 0) {
        row += "  " + hbar_cell(s_.by_weekday[w], day_max, bar_cells,
                                wd == peak_weekday() ? kPeak : kBar);
      }
      line(rtrim(row));
    }
    std::vector<std::pair<std::size_t, std::string>> axis;
    for (int h = 0; h < 24; h += 3) {
      char label[8];
      std::snprintf(label, sizeof label, "%02d", h);
      axis.push_back({static_cast<std::size_t>(h) * 2, label});
    }
    line(paint_(kLabel, spaces(gutter) + place_labels(axis, 48)));

    struct Phase {
      const char* name;
      int from;
      int to;
    };
    const Phase phases[] = {{"night", 0, 5}, {"morning", 5, 12}, {"afternoon", 12, 17},
                            {"evening", 17, 24}};
    std::int64_t counts[4] = {0, 0, 0, 0};
    std::size_t best = 0;
    for (std::size_t i = 0; i < 4; ++i) {
      for (int h = phases[i].from; h < phases[i].to; ++h) {
        counts[i] += s_.by_hour[static_cast<std::size_t>(h)];
      }
      if (counts[i] > counts[best]) best = i;
    }
    std::vector<Piece> parts;
    for (std::size_t i = 0; i < 4; ++i) {
      parts.push_back(labelled(phases[i].name, format_percent(share(counts[i], s_.commits)),
                               i == best ? kHighlight : kStrong));
    }
    line();
    flow_lines(parts, g_.dot);
    flow_lines({labelled("busiest day", kWeekdayLong[peak_weekday()]),
                labelled("busiest hour", hour_text(peak_hour())),
                labelled("weekends", format_percent(share(s_.weekend_commits, s_.commits))),
                labelled("Friday after 4pm", format_number(s_.friday_evening_commits))},
               g_.dot);
  }

  void highlights() {
    section("Highlights");
    field("Busiest day",
          paint_(kStrong, std::string(weekday_name(weekday_from_days(s_.busiest_day))) + ", " +
                              format_date(s_.busiest_day)) +
              paint_(kFaint, g_.dot) + count_of(s_.busiest_day_commits, "commit", "commits"));
    if (s_.span_days() > 7) {
      field("Busiest week", "week of " + paint_(kStrong, format_date(s_.busiest_week_start)) +
                                paint_(kFaint, g_.dot) +
                                count_of(s_.busiest_week_commits, "commit", "commits"));
    }
    if (s_.span_days() > 31) {
      field("Busiest month", paint_(kStrong, format_month(s_.busiest_month_day)) +
                                 paint_(kFaint, g_.dot) +
                                 count_of(s_.busiest_month_commits, "commit", "commits"));
    }
    std::string streak = paint_(kStrong, count_of(s_.longest_streak, "day", "days"));
    if (s_.longest_streak > 1) {
      streak += paint_(kFaint, g_.dot) + short_date(s_.streak_start) + paint_(kFaint, g_.arrow) +
                short_date(s_.streak_end);
    }
    field("Longest streak", streak);
    if (s_.longest_gap_seconds > 0) {
      field("Longest break", paint_(kStrong, format_duration(s_.longest_gap_seconds)) +
                                 paint_(kFaint, g_.dot) +
                                 short_date(floor_div(s_.gap_from, kDay)) +
                                 paint_(kFaint, g_.arrow) + short_date(floor_div(s_.gap_to, kDay)));
    }
    if (s_.has_biggest_commit) {
      const CommitRef& c = s_.biggest_commit;
      const std::string size = "+" + format_number(c.added) + " -" + format_number(c.deleted);
      const std::size_t used = 16 + size.size() + 3 + 7 + 1;
      field("Biggest commit", lines_changed(c.added, c.deleted) + paint_(kFaint, g_.dot) +
                                  paint_(kYellow, short_hash(c.hash)) + " " +
                                  quoted(c.subject, inner_ > used + 14 ? inner_ - used - 2 : 12));
    }
    field("Night owl", paint_(kStrong, count_of(s_.night_commits, "commit", "commits")) +
                           paint_(kLabel, " between 00:00 and 05:00 (" +
                                              format_percent(share(s_.night_commits, s_.commits)) +
                                              ")"));
    for (const CommitRef& c : s_.night_examples) {
      const std::int64_t local = c.time + c.tz_offset;
      const std::string when = short_date(floor_div(local, kDay)) + " " +
                               format_clock(static_cast<int>(floor_mod(local, kDay)));
      const std::size_t used = 16 + display_width(when) + 1 + 7 + 1;
      line(spaces(16) + paint_(kLabel, when) + " " + paint_(kYellow, short_hash(c.hash)) + " " +
           quoted(c.subject, inner_ > used + 14 ? inner_ - used - 2 : 12));
    }
  }

  void authors() {
    section("Authors");
    const std::size_t shown = std::min<std::size_t>(s_.authors.size(), top_);
    std::size_t name_width = 6;
    for (std::size_t i = 0; i < shown; ++i) {
      name_width = std::max(name_width, display_width(display_name(s_.authors[i])));
    }
    name_width = std::min<std::size_t>(name_width, 22);
    std::size_t number_width = 1;
    std::size_t lines_width = 0;
    for (std::size_t i = 0; i < shown; ++i) {
      const AuthorStat& a = s_.authors[i];
      number_width = std::max(number_width, format_number(a.commits).size());
      lines_width = std::max(lines_width, format_number(a.added).size() +
                                              format_number(a.deleted).size() + 3);
    }
    const std::size_t rank_width = std::to_string(shown).size();
    // rank, name, bar, commits, share, lines
    const std::size_t fixed = rank_width + 2 + 2 + 1 + number_width + 6 + 2 + lines_width;
    std::size_t bar_cells = 24;
    if (fixed + name_width + bar_cells > inner_) {
      const std::size_t room = inner_ > fixed ? inner_ - fixed : 0;
      bar_cells = std::clamp<std::size_t>(room > name_width ? room - name_width : 0, 4, 24);
      if (fixed + name_width + bar_cells > inner_) {
        name_width = std::max<std::size_t>(6, room > bar_cells ? room - bar_cells : 6);
      }
    }
    const std::int64_t max = s_.authors.empty() ? 0 : s_.authors.front().commits;
    for (std::size_t i = 0; i < shown; ++i) {
      const AuthorStat& a = s_.authors[i];
      const std::string name =
          pad_right(truncate_right(display_name(a), name_width, g_.ellipsis), name_width);
      std::string text = paint_(kFaint, pad_left(std::to_string(i + 1), rank_width)) + "  " +
                         paint_(i == 0 ? kStrong : "", name) + "  " +
                         hbar_cell(a.commits, max, bar_cells, i == 0 ? kPeak : kBar) + " " +
                         paint_(kStrong, pad_left(format_number(a.commits), number_width)) +
                         paint_(kLabel, pad_left(format_percent(share(a.commits, s_.commits)), 6)) +
                         "  " + lines_changed(a.added, a.deleted);
      line(text);
    }
    if (s_.authors.size() > shown) {
      line(paint_(kLabel, "... and " +
                              count_of(static_cast<std::int64_t>(s_.authors.size() - shown),
                                       "more author", "more authors")));
    }
  }

  void languages() {
    if (s_.languages.empty()) return;
    std::int64_t total = 0;
    bool by_lines = true;
    for (const LanguageStat& l : s_.languages) total += l.lines;
    if (total == 0) {
      by_lines = false;
      for (const LanguageStat& l : s_.languages) total += l.changes;
    }
    if (total == 0) return;
    section("Languages");

    // Largest-remainder split of the bar so the segments always add up exactly.
    const std::size_t width = inner_;
    std::vector<std::size_t> cells(s_.languages.size(), 0);
    std::vector<std::pair<double, std::size_t>> remainders;
    std::size_t used = 0;
    for (std::size_t i = 0; i < s_.languages.size(); ++i) {
      const std::int64_t weight = by_lines ? s_.languages[i].lines : s_.languages[i].changes;
      const double exact = static_cast<double>(weight) / static_cast<double>(total) *
                           static_cast<double>(width);
      cells[i] = static_cast<std::size_t>(exact);
      used += cells[i];
      remainders.push_back({exact - static_cast<double>(cells[i]), i});
    }
    std::sort(remainders.begin(), remainders.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    for (std::size_t k = 0; used < width && k < remainders.size(); ++k, ++used) {
      ++cells[remainders[k].second];
    }

    std::string bar;
    std::vector<Piece> legend;
    for (std::size_t i = 0; i < s_.languages.size(); ++i) {
      const LanguageStat& l = s_.languages[i];
      const std::string style = "38;5;" + std::to_string(l.ansi);
      const char* fill = paint_.on ? g_.full : g_.fills[i % 4];
      bar += paint_(style, repeat(fill, cells[i]));
      const std::int64_t weight = by_lines ? l.lines : l.changes;
      const std::string pct = percent1(share(weight, total));
      const std::string marker = paint_.on ? g_.marker : g_.fills[i % 4];
      legend.push_back({paint_(style, marker) + " " + paint_(kStrong, l.name) + " " +
                            paint_(kLabel, pct),
                        display_width(marker) + 1 + display_width(l.name) + 1 + pct.size()});
    }
    line(bar);
    for (const std::string& text : flow(legend, "   ", 3, inner_)) line(text);
  }

  void files() {
    if (s_.top_files.empty()) return;
    section("Hot files");
    std::size_t longest = 8;
    std::size_t number_width = 1;
    std::size_t lines_width = 0;
    for (const FileStat& f : s_.top_files) {
      longest = std::max(longest, display_width(f.path));
      number_width = std::max(number_width, format_number(f.commits).size());
      lines_width = std::max(lines_width,
                             format_number(f.added).size() + format_number(f.deleted).size() + 3);
    }
    const std::size_t bar_cells = inner_ >= 90 ? 14 : 8;
    const std::size_t fixed = 2 + bar_cells + 1 + number_width + 10 + lines_width;
    const std::size_t path_width = std::min(longest, std::max<std::size_t>(12, inner_ > fixed
                                                                                   ? inner_ - fixed
                                                                                   : 12));
    const std::int64_t max = s_.top_files.front().commits;
    for (std::size_t i = 0; i < s_.top_files.size(); ++i) {
      const FileStat& f = s_.top_files[i];
      const std::string path = truncate_left(f.path, path_width, g_.ellipsis);
      line(paint_(i == 0 ? kStrong : "", path) + spaces(path_width - display_width(path)) + "  " +
           hbar_cell(f.commits, max, bar_cells, i == 0 ? kPeak : kBar) + " " +
           paint_(kStrong, pad_left(format_number(f.commits), number_width)) +
           paint_(kLabel, f.commits == 1 ? " commit   " : " commits  ") +
           lines_changed(f.added, f.deleted));
    }
  }

  void words() {
    if (s_.top_words.empty()) return;
    section("Words");
    std::vector<Piece> pieces;
    for (std::size_t i = 0; i < s_.top_words.size(); ++i) {
      const CountItem& w = s_.top_words[i];
      const std::string label = truncate_right(w.label, 24, g_.ellipsis);
      const std::string count = format_number(w.count);
      pieces.push_back({paint_(i < 3 ? kHighlight : kStrong, label) + " " + paint_(kLabel, count),
                        display_width(label) + 1 + count.size()});
    }
    for (const std::string& text : flow(pieces, "   ", 3, inner_)) line(text);
  }

  void messages() {
    if (s_.messages == 0) return;
    section("Messages");
    char avg[32];
    std::snprintf(avg, sizeof avg, "%.0f", s_.avg_subject_length);
    const char* verdict = s_.avg_subject_length < 15   ? "terse"
                          : s_.avg_subject_length < 50 ? "to the point"
                                                       : "detailed";
    field("Average length",
          paint_(kStrong, std::string(avg) + " characters") + paint_(kLabel, "  (" + std::string(verdict) + ")"));

    // Appends "a, b, c" after `head` for as many items as fit on the line.
    const auto with_details = [&](const std::string& head, std::size_t head_width,
                                  const std::vector<std::string>& items) {
      std::string out = head;
      std::size_t used = 16 + head_width;
      std::string list;
      for (const std::string& item : items) {
        const std::size_t extra = display_width(item) + 2;
        if (used + extra + 1 > inner_) break;
        list += (list.empty() ? "" : ", ") + item;
        used += extra;
      }
      if (!list.empty()) out += paint_(kLabel, "  " + list);
      return out;
    };

    const std::string conventional_pct = format_percent(share(s_.conventional_commits, s_.messages));
    std::vector<std::string> types;
    for (std::size_t i = 0; i < std::min<std::size_t>(s_.commit_types.size(), 5); ++i) {
      types.push_back(s_.commit_types[i].label + " " + format_number(s_.commit_types[i].count));
    }
    field("Conventional",
          with_details(paint_(kStrong, conventional_pct) + paint_(kLabel, " of commits"),
                       conventional_pct.size() + 11, types));

    const std::string lazy_head = format_number(s_.lazy_messages) + " (" +
                                  format_percent(share(s_.lazy_messages, s_.messages)) + ")";
    std::vector<std::string> lazy;
    for (const CountItem& item : s_.lazy_top) {
      lazy.push_back("\"" + truncate_right(item.label, 16, g_.ellipsis) + "\" x" +
                     format_number(item.count));
    }
    field("Low-effort", with_details(paint_(kStrong, lazy_head), display_width(lazy_head), lazy));
    if (s_.reverts > 0) field("Reverts", paint_(kStrong, format_number(s_.reverts)));
  }

  void roast() {
    section("Roast");
    for (const std::string& text : build_roast(s_)) {
      Prose p(paint_);
      p.add(text);
      bool first = true;
      for (const std::string& wrapped : p.wrap(inner_ - 2)) {
        line((first ? paint_(kYellow, g_.bullet) : std::string("  ")) + wrapped);
        first = false;
      }
    }
  }

  void footer() {
    out_ += '\n';
    line(paint_(kFaint, repeat(g_.rule, inner_)));
    const std::string left = "gitstory " GITSTORY_VERSION;
    const std::string right = kProjectUrl;
    const std::size_t gap = inner_ > left.size() + right.size() ? inner_ - left.size() - right.size()
                                                                : 2;
    line(paint_(kFaint, left + spaces(gap) + right));
    out_ += '\n';
  }

  const Stats& s_;
  const RepoInfo& repo_;
  const TermOptions& o_;
  const Glyphs& g_;
  Painter paint_;
  std::size_t width_ = 80;
  std::size_t inner_ = 76;
  std::size_t top_ = 10;
  bool one_year_ = true;
  std::string out_;
};

}  // namespace

std::string render_terminal(const Stats& stats, const RepoInfo& repo, const TermOptions& options) {
  if (stats.commits == 0) return "No commits found.\n";
  return Report(stats, repo, options).build();
}

}  // namespace gitstory
