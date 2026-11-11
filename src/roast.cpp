#include "analyze.hpp"

#include "util.hpp"

namespace gitstory {

namespace {

double ratio(std::int64_t part, std::int64_t whole) {
  return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0.0;
}

}  // namespace

// Every line is backed by a threshold on a real number, so a tidy repository
// gets a short roast and a chaotic one gets a long one.
std::vector<std::string> build_roast(const Stats& s, Lang lang) {
  std::vector<std::string> lines;
  if (s.commits == 0) return lines;
  const Locale loc(lang);
  const Strings& t = loc.text();

  const double night = ratio(s.night_commits, s.commits);
  if (s.night_commits >= 3 && night >= 0.10) {
    lines.push_back(fill(t.roast_night, {loc.percent(night)}));
  } else if (s.night_commits == 0 && s.commits >= 50) {
    lines.push_back(t.roast_healthy);
  }

  const double lazy = ratio(s.lazy_messages, s.messages);
  if (s.lazy_messages >= 3 && lazy >= 0.05) {
    std::string top;
    if (!s.lazy_top.empty()) {
      top = fill(t.roast_lazy_top,
                 {s.lazy_top.front().label, loc.count(s.lazy_top.front().count, t.time)});
    }
    lines.push_back(fill(t.roast_lazy, {loc.count(s.lazy_messages, t.message), top}));
  }

  if (s.messages >= 10 && s.avg_subject_length < 15.0) {
    const auto average = static_cast<std::int64_t>(s.avg_subject_length + 0.5);
    lines.push_back(
        fill(t.roast_short, {loc.number(average), loc.count(average, t.character)}));
  }

  if (s.longest_gap_seconds >= 30 * 86400) {
    lines.push_back(fill(t.roast_silent, {loc.duration(s.longest_gap_seconds),
                                          loc.date(floor_div(s.gap_from, 86400))}));
  }

  if (s.has_biggest_commit) {
    const std::int64_t size = s.biggest_commit.added + s.biggest_commit.deleted;
    if (size >= 2000) lines.push_back(fill(t.roast_big, {loc.count(size, t.line)}));
  }

  const double weekend = ratio(s.weekend_commits, s.commits);
  if (s.commits >= 10 && weekend >= 0.30) {
    lines.push_back(fill(t.roast_weekend, {loc.percent(weekend)}));
  }

  if (s.friday_evening_commits >= 3) {
    lines.push_back(fill(t.roast_friday, {loc.count(s.friday_evening_commits, t.commit)}));
  }

  if (!s.top_files.empty() && s.commits >= 20) {
    const FileStat& hot = s.top_files.front();
    const double share = ratio(hot.commits, s.commits);
    if (share >= 0.25) lines.push_back(fill(t.roast_hot_file, {hot.path, loc.percent(share)}));
  }

  if (!s.authors.empty() && s.commits >= 20) {
    const AuthorStat& top = s.authors.front();
    const double share = ratio(top.commits, s.commits);
    if (s.author_count == 1) {
      lines.push_back(fill(t.roast_solo, {loc.count(s.commits, t.commit)}));
    } else if (share >= 0.80) {
      lines.push_back(fill(t.roast_dominant, {top.name, loc.percent(share)}));
    }
  }

  if (s.reverts >= 3) lines.push_back(fill(t.roast_reverts, {loc.count(s.reverts, t.revert)}));

  if (s.longest_streak >= 14) {
    lines.push_back(fill(t.roast_streak,
                         {loc.number(s.longest_streak), loc.count(s.longest_streak, t.day)}));
  }

  if (s.messages >= 20 && ratio(s.conventional_commits, s.messages) >= 0.90) {
    lines.push_back(t.roast_conventional);
  }

  if (lines.empty()) lines.push_back(t.roast_nothing);
  return lines;
}

}  // namespace gitstory
