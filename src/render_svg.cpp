#include "render.hpp"

#include <algorithm>
#include <cstdio>

#include "util.hpp"

namespace gitstory {

std::string xml_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
  return out;
}

namespace {

struct Theme {
  const char* background;
  const char* border;
  const char* panel;
  const char* text;
  const char* muted;
  const char* accent;
  const char* heat[5];
};

constexpr Theme kDark = {"#0d1117", "#30363d", "#161b22", "#e6edf3", "#8b949e", "#58a6ff",
                         {"#1b222c", "#0e4429", "#006d32", "#26a641", "#39d353"}};
constexpr Theme kLight = {"#ffffff", "#d0d7de", "#f6f8fa", "#1f2328", "#656d76", "#0969da",
                          {"#ebedf0", "#9be9a8", "#40c463", "#30a14e", "#216e39"}};

std::string num(double value) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.1f", value);
  std::string out = buf;
  for (char& c : out) {
    if (c == ',') c = '.';  // never let a locale leak into the markup
  }
  if (out.size() > 2 && out.compare(out.size() - 2, 2, ".0") == 0) out.erase(out.size() - 2);
  return out;
}

// 12345 -> "12.3k", 2500000 -> "2.5M": tiles have room for only a few glyphs.
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

class Card {
 public:
  Card(const Stats& stats, const RepoInfo& repo, const Theme& theme)
      : s_(stats), repo_(repo), t_(theme) {}

  std::string build() {
    out_ += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"520\" "
            "viewBox=\"0 0 800 520\" role=\"img\" aria-label=\"gitstory card for " +
            xml_escape(repo_.name) + "\">\n";
    out_ += "  <title>gitstory: " + xml_escape(repo_.name) + "</title>\n";
    out_ += "  <style>text{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,"
            "Arial,sans-serif}</style>\n";
    rect(0.5, 0.5, 799, 519, t_.background, 10, t_.border);
    header();
    tiles();
    heatmap();
    highlights();
    timeline();
    languages();
    footer();
    out_ += "</svg>\n";
    return std::move(out_);
  }

 private:
  void rect(double x, double y, double w, double h, const char* fill, double radius = 0,
            const char* stroke = nullptr) {
    out_ += "  <rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(w) +
            "\" height=\"" + num(h) + "\" fill=\"" + fill + "\"";
    if (radius > 0) out_ += " rx=\"" + num(radius) + "\"";
    if (stroke != nullptr) out_ += std::string(" stroke=\"") + stroke + "\"";
    out_ += "/>\n";
  }

  void text(double x, double y, const std::string& content, int size, const char* fill,
            const char* weight = "400", const char* anchor = "start") {
    out_ += "  <text x=\"" + num(x) + "\" y=\"" + num(y) + "\" font-size=\"" +
            std::to_string(size) + "\" font-weight=\"" + weight + "\" fill=\"" + fill +
            "\" text-anchor=\"" + anchor + "\">" + xml_escape(content) + "</text>\n";
  }

  static double share(std::int64_t part, std::int64_t whole) {
    return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0.0;
  }

  void header() {
    text(24, 44, truncate_right(repo_.name, 38, "…"), 24, t_.text, "600");
    std::string span = format_date(s_.first_day);
    if (s_.last_day != s_.first_day) span += " – " + format_date(s_.last_day);
    text(24, 66, span + "  ·  " + format_span_days(s_.span_days()), 13, t_.muted);
    text(776, 44, "gitstory", 14, t_.accent, "600", "end");
  }

  void tiles() {
    struct Tile {
      std::string value;
      const char* label;
    };
    const Tile tiles[] = {
        {compact(s_.commits), "commits"},
        {compact(s_.author_count), s_.author_count == 1 ? "author" : "authors"},
        {compact(s_.active_days), "active days"},
        {compact(s_.added + s_.deleted), "lines changed"},
    };
    const double width = 176;
    const double gap = 16;
    for (int i = 0; i < 4; ++i) {
      const double x = 24 + i * (width + gap);
      rect(x, 84, width, 64, t_.panel, 8, t_.border);
      text(x + 14, 114, tiles[i].value, 22, t_.text, "600");
      text(x + 14, 134, tiles[i].label, 12, t_.muted);
    }
  }

  void heatmap() {
    const double left = 62;
    const double top = 196;
    const double step = 16;
    text(24, 180, "When commits happen", 13, t_.text, "600");
    std::int64_t max = 0;
    for (const auto& row : s_.heatmap) max = std::max(max, *std::max_element(row.begin(), row.end()));
    for (int wd = 0; wd < 7; ++wd) {
      text(24, top + wd * step + 11, weekday_name(wd), 11, t_.muted);
      for (int h = 0; h < 24; ++h) {
        const std::int64_t v = s_.heatmap[static_cast<std::size_t>(wd)][static_cast<std::size_t>(h)];
        int level = 0;
        if (v > 0 && max > 0) level = static_cast<int>((v * 4 - 1) / max) + 1;
        rect(left + h * step, top + wd * step, 13, 13, t_.heat[level], 2.5);
      }
    }
    for (int h = 0; h < 24; h += 3) {
      char label[8];
      std::snprintf(label, sizeof label, "%02d", h);
      text(left + h * step, top + 7 * step + 12, label, 10, t_.muted);
    }
  }

  void highlights() {
    const double x = 476;
    double y = 180;
    text(x, y, "Highlights", 13, t_.text, "600");
    const auto row = [&](const char* label, const std::string& value) {
      y += 23;
      text(x, y, label, 12, t_.muted);
      text(776, y, value, 12, t_.text, "400", "end");
    };
    row("Busiest day", format_date(s_.busiest_day) + " (" + format_number(s_.busiest_day_commits) +
                           ")");
    row("Longest streak", format_number(s_.longest_streak) +
                              (s_.longest_streak == 1 ? " day" : " days"));
    row("Longest break", s_.longest_gap_seconds > 0 ? format_duration(s_.longest_gap_seconds)
                                                     : std::string("-"));
    row("Night commits (00-05)", format_percent(share(s_.night_commits, s_.commits)));
    row("Weekend commits", format_percent(share(s_.weekend_commits, s_.commits)));
    const auto peak = static_cast<int>(
        std::max_element(s_.by_hour.begin(), s_.by_hour.end()) - s_.by_hour.begin());
    char hour[16];
    std::snprintf(hour, sizeof hour, "%02d:00", peak);
    row("Peak hour", hour);
  }

  void timeline() {
    const double left = 24;
    const double base = 418;
    const double height = 56;
    const double width = 752;
    text(left, 350, "Activity over time", 13, t_.text, "600");
    const Timeline timeline = bucket_timeline(s_, 94);
    if (timeline.buckets.empty()) return;
    const std::int64_t max = *std::max_element(timeline.buckets.begin(), timeline.buckets.end());
    const double step = width / static_cast<double>(timeline.buckets.size());
    const double bar = std::max(1.0, std::min(step - 2.0, 24.0));
    for (std::size_t i = 0; i < timeline.buckets.size(); ++i) {
      const std::int64_t v = timeline.buckets[i];
      const double x = left + static_cast<double>(i) * step + (step - bar) / 2.0;
      if (v <= 0 || max <= 0) {
        rect(x, base - 2, bar, 2, t_.heat[0], 1);
        continue;
      }
      const double h = std::max(3.0, static_cast<double>(v) / static_cast<double>(max) * height);
      rect(x, base - h, bar, h, t_.heat[3], 1.5);
    }
    text(left, base + 16, format_month(s_.first_day), 10, t_.muted);
    if (s_.last_day != s_.first_day) {
      text(left + width, base + 16, format_month(s_.last_day), 10, t_.muted, "400", "end");
    }
  }

  void languages() {
    std::int64_t total = 0;
    for (const LanguageStat& l : s_.languages) total += l.lines;
    if (total <= 0) return;
    const double left = 24;
    const double width = 752;
    const double y = 452;
    // Clip the segments to a rounded track so the bar has soft ends.
    out_ += "  <clipPath id=\"langs\"><rect x=\"" + num(left) + "\" y=\"" + num(y) +
            "\" width=\"" + num(width) + "\" height=\"8\" rx=\"4\"/></clipPath>\n";
    out_ += "  <g clip-path=\"url(#langs)\">\n";
    double x = left;
    for (const LanguageStat& l : s_.languages) {
      const double w = static_cast<double>(l.lines) / static_cast<double>(total) * width;
      if (w <= 0) continue;
      out_ += "  ";
      rect(x, y, w, 8, l.hex.c_str());
      x += w;
    }
    out_ += "  </g>\n";

    double lx = left;
    for (const LanguageStat& l : s_.languages) {
      const double pct = static_cast<double>(l.lines) / static_cast<double>(total) * 100.0;
      if (pct < 0.05) continue;
      char share[32];
      std::snprintf(share, sizeof share, "%.1f%%", pct);
      std::string label = l.name + " " + share;
      for (char& c : label) {
        if (c == ',') c = '.';
      }
      const double item = 18 + static_cast<double>(display_width(label)) * 6.6;
      if (lx + item > left + width) break;
      out_ += "  <circle cx=\"" + num(lx + 4) + "\" cy=\"" + num(y + 25) + "\" r=\"4\" fill=\"" +
              l.hex + "\"/>\n";
      text(lx + 13, y + 29, label, 11, t_.muted);
      lx += item + 14;
    }
  }

  void footer() {
    if (s_.top_words.empty()) return;
    std::string words;
    const std::size_t n = std::min<std::size_t>(s_.top_words.size(), 6);
    for (std::size_t i = 0; i < n; ++i) {
      if (i != 0) words += "  ·  ";
      words += truncate_right(s_.top_words[i].label, 16, "…");
    }
    text(24, 506, "Most used words:  " + words, 11, t_.muted);
  }

  const Stats& s_;
  const RepoInfo& repo_;
  const Theme& t_;
  std::string out_;
};

}  // namespace

std::string render_svg(const Stats& stats, const RepoInfo& repo, const SvgOptions& options) {
  return Card(stats, repo, options.dark ? kDark : kLight).build();
}

}  // namespace gitstory
