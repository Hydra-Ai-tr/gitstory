#include "analyze.hpp"

#include <algorithm>
#include <unordered_set>

#include "util.hpp"

namespace gitstory {

namespace {

constexpr std::int64_t kDay = 86400;
constexpr std::size_t kNightExamples = 3;
constexpr int kNightEndHour = 5;

bool is_word_char(char32_t cp) {
  if (cp < 0x80) {
    return (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z');
  }
  // Non-ASCII: assume a letter unless it sits in a punctuation/symbol block.
  if (cp <= 0xBF || cp == 0xD7 || cp == 0xF7) return false;
  if (cp >= 0x2000 && cp <= 0x2BFF) return false;    // punctuation, arrows, symbols
  if (cp >= 0x3000 && cp <= 0x303F) return false;    // CJK punctuation
  if (cp >= 0xE000 && cp <= 0xF8FF) return false;    // private use
  if (cp >= 0xFE00 && cp <= 0xFE6F) return false;    // variation selectors, small forms
  if (cp >= 0xFF00 && cp <= 0xFF0F) return false;    // fullwidth punctuation
  if (cp >= 0xFF1A && cp <= 0xFF20) return false;
  if (cp == 0xFFFD) return false;
  if (cp >= 0x1F000 && cp <= 0x1FAFF) return false;  // emoji
  return true;
}

const std::unordered_set<std::string>& stop_words() {
  static const std::unordered_set<std::string> words = {
      // English
      "the", "and", "for", "with", "from", "into", "that", "this", "are", "was", "not", "but",
      "all", "can", "has", "have", "its", "when", "via", "out", "now", "too", "also", "some",
      "more", "will", "should", "been", "were", "our", "you", "your", "their", "them", "then",
      "than", "off", "per", "etc", "use", "one", "two", "does", "did", "don", "isn", "any",
      "about", "after", "before", "over", "under", "which", "while", "would", "could", "there",
      "these", "those", "just", "only", "still", "again", "how", "why", "what", "who",
      // Turkish
      "bir", "ile", "için", "icin", "olan", "olarak", "gibi", "daha", "çok", "cok", "ama",
      "veya", "hem", "kadar", "sonra", "önce", "once", "artık", "artik", "bazı", "bazi", "her",
      "şey", "sey", "biraz", "diye", "oldu", "var", "yok", "ise", "değil", "degil", "tüm", "tum",
      "bütün", "butun", "yani", "hiç", "hic", "şimdi", "simdi",
      // Russian
      "для", "что", "это", "как", "при", "без", "или", "так", "все", "всё", "уже", "ещё", "еще",
      "его", "они", "над", "под", "про", "чем", "где", "там", "тут", "после", "перед", "через",
      "также", "только", "теперь", "более",
  };
  return words;
}

const std::unordered_set<std::string>& lazy_messages() {
  static const std::unordered_set<std::string> messages = {
      "fix", "fixes", "fixed", "fix bug", "bug fix", "bugfix", "bug fixes", "bugfixes",
      "small fix", "minor fix", "quick fix", "fix typo", "typo", "wip", "update", "updates",
      "updated", "change", "changes", "minor changes", "small changes", "some changes", "stuff",
      "things", "test", "testing", "tmp", "temp", "asdf", "asd", "qwe", "minor", "misc",
      "cleanup", "clean up", "oops", "done", "commit", "save", "work", "progress", "more",
      "more changes", "more fixes", "final", "try", "try again", "again", "debug", "hotfix",
      "patch", "edit", "edits", "refactor", "improvements", "tweaks", "tweak", "up",
      // Turkish
      "düzeltme", "duzeltme", "düzeltmeler", "güncelleme", "guncelleme", "deneme", "değişiklik",
      "degisiklik", "değişiklikler", "son", "bitti", "tamam",
  };
  return messages;
}

const std::unordered_set<std::string>& conventional_types() {
  static const std::unordered_set<std::string> types = {
      "feat", "fix", "docs", "style", "refactor", "perf", "test", "tests",
      "build", "ci", "chore", "revert", "deps", "release", "wip", "hotfix",
  };
  return types;
}

bool looks_like_hash(const std::string& word) {
  if (word.size() < 7) return false;
  bool has_digit = false;
  for (const char c : word) {
    if (c >= '0' && c <= '9') {
      has_digit = true;
    } else if (c < 'a' || c > 'f') {
      return false;
    }
  }
  return has_digit;
}

bool all_digits(const std::string& word) {
  return std::all_of(word.begin(), word.end(), [](char c) { return c >= '0' && c <= '9'; });
}

template <typename Map>
std::vector<CountItem> top_counts(const Map& map, std::size_t limit) {
  std::vector<CountItem> items;
  items.reserve(map.size());
  for (const auto& [label, count] : map) items.push_back({label, count});
  // Ties break alphabetically so output never depends on hash-map order.
  std::sort(items.begin(), items.end(), [](const CountItem& a, const CountItem& b) {
    return a.count != b.count ? a.count > b.count : a.label < b.label;
  });
  if (items.size() > limit) items.resize(limit);
  return items;
}

struct LanguageDef {
  const char* name;
  int ansi;
  const char* hex;
  const char* extensions;  // space separated
};

// Colours follow GitHub's linguist palette, nudged where the original would
// vanish on a dark background.
constexpr LanguageDef kLanguages[] = {
    {"JavaScript", 221, "#f1e05a", ".js .mjs .cjs .jsx"},
    {"TypeScript", 75, "#3178c6", ".ts .tsx .mts .cts"},
    {"Python", 74, "#3572a5", ".py .pyw .pyi .ipynb"},
    {"C++", 168, "#f34b7d", ".cpp .cc .cxx .hpp .hh .hxx .ipp .inl"},
    {"C", 250, "#a8b3bd", ".c .h"},
    {"C#", 34, "#178600", ".cs .csx"},
    {"Java", 172, "#b07219", ".java"},
    {"Kotlin", 135, "#a97bff", ".kt .kts"},
    {"Go", 80, "#00add8", ".go"},
    {"Rust", 173, "#dea584", ".rs"},
    {"Ruby", 160, "#cc342d", ".rb .erb"},
    {"PHP", 104, "#7a86b8", ".php"},
    {"Swift", 209, "#f05138", ".swift"},
    {"Dart", 38, "#00b4ab", ".dart"},
    {"Lua", 69, "#5c7cfa", ".lua"},
    {"HTML", 202, "#e34c26", ".html .htm .xhtml"},
    {"CSS", 98, "#8a63d2", ".css .less"},
    {"SCSS", 169, "#c6538c", ".scss .sass"},
    {"Vue", 78, "#41b883", ".vue"},
    {"Svelte", 203, "#ff3e00", ".svelte"},
    {"Shell", 113, "#89e051", ".sh .bash .zsh .fish"},
    {"PowerShell", 68, "#5391fe", ".ps1 .psm1 .psd1"},
    {"SQL", 179, "#e38c00", ".sql"},
    {"CMake", 167, "#da3434", ".cmake"},
    {"JSON", 244, "#8b949e", ".json .jsonc"},
    {"YAML", 131, "#cb5c5c", ".yml .yaml"},
    {"TOML", 137, "#b9734b", ".toml"},
    {"XML", 67, "#3d8fd1", ".xml .xaml .plist"},
    {"Markdown", 252, "#c9d1d9", ".md .markdown .mdx .rst"},
    {"Text", 248, "#a5adb5", ".txt"},
    {"Lockfiles", 243, "#7d8590", ".lock .sum"},
    {"Images", 140, "#a371f7", ".png .jpg .jpeg .gif .svg .ico .webp .bmp"},
};

constexpr std::size_t kLanguageSlots = 6;

CommitRef make_ref(const Commit& commit, std::int64_t added, std::int64_t deleted) {
  CommitRef ref;
  ref.hash = commit.hash;
  ref.subject = commit.subject;
  ref.author = commit.author_name;
  ref.time = commit.time;
  ref.tz_offset = commit.tz_offset;
  ref.added = added;
  ref.deleted = deleted;
  return ref;
}

}  // namespace

// ---- text helpers ----------------------------------------------------------

std::vector<std::string> tokenize_words(std::string_view subject) {
  std::vector<std::string> out;
  std::string word;
  std::size_t length = 0;
  const auto flush = [&] {
    if (length >= 3 && length <= 40 && !all_digits(word) && !looks_like_hash(word) &&
        stop_words().count(word) == 0) {
      out.push_back(word);
    }
    word.clear();
    length = 0;
  };
  std::size_t i = 0;
  while (i < subject.size()) {
    char32_t cp = 0;
    decode_utf8(subject, i, cp);
    if (is_word_char(cp)) {
      append_utf8(word, to_lower(cp));
      ++length;
    } else if (cp == '\'' || cp == 0x2019) {
      // Apostrophes neither split nor join: "don't" -> "dont".
    } else {
      flush();
    }
  }
  flush();
  return out;
}

std::string normalize_message(std::string_view subject) {
  std::string text = lower_utf8(trim(subject));
  while (!text.empty() && (text.back() == '.' || text.back() == '!' || text.back() == ' ')) {
    text.pop_back();
  }
  return text;
}

bool is_lazy_message(std::string_view subject) {
  const std::string text = normalize_message(subject);
  return codepoint_count(text) <= 2 || lazy_messages().count(text) != 0;
}

std::string conventional_type(std::string_view subject) {
  std::size_t i = 0;
  std::string type;
  while (i < subject.size() &&
         ((subject[i] >= 'a' && subject[i] <= 'z') || (subject[i] >= 'A' && subject[i] <= 'Z'))) {
    type += static_cast<char>(subject[i] | 0x20);
    ++i;
  }
  if (type.empty()) return {};
  if (i < subject.size() && subject[i] == '(') {
    const std::size_t close = subject.find(')', i);
    if (close == std::string_view::npos) return {};
    i = close + 1;
  }
  if (i < subject.size() && subject[i] == '!') ++i;
  if (i >= subject.size() || subject[i] != ':') return {};
  return conventional_types().count(type) != 0 ? type : std::string();
}

std::string file_extension(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
  const std::size_t dot = name.find_last_of('.');
  // No dot, a dotfile such as ".gitignore", or a trailing dot: no extension.
  if (dot == std::string_view::npos || dot == 0 || dot + 1 == name.size()) return "(none)";
  return lower_utf8(name.substr(dot));
}

LanguageStat language_for_extension(std::string_view extension) {
  LanguageStat language;
  language.name = "Other";
  language.ansi = 242;
  language.hex = "#6e7681";
  if (extension.empty() || extension.front() != '.') return language;
  for (const LanguageDef& def : kLanguages) {
    for (std::string_view known : split(def.extensions, ' ')) {
      if (known == extension) {
        language.name = def.name;
        language.ansi = def.ansi;
        language.hex = def.hex;
        return language;
      }
    }
  }
  return language;
}

// ---- Analyzer --------------------------------------------------------------

Analyzer::Analyzer(std::size_t top_n) : top_n_(top_n == 0 ? 1 : top_n) {}

void Analyzer::add(const Commit& commit) {
  Stats& s = stats_;
  const std::int64_t local = commit.local_time();
  const std::int64_t day = floor_div(local, kDay);
  const int hour = static_cast<int>(floor_mod(local, kDay) / 3600);
  const int weekday = weekday_from_days(day);

  if (s.commits == 0) {
    s.first_time = s.last_time = commit.time;
    s.first_day = s.last_day = day;
  } else {
    s.first_time = std::min(s.first_time, commit.time);
    s.last_time = std::max(s.last_time, commit.time);
    s.first_day = std::min(s.first_day, day);
    s.last_day = std::max(s.last_day, day);
  }
  ++s.commits;
  if (commit.is_merge()) ++s.merges;
  times_.push_back(commit.time);
  ++s.daily[day];

  ++s.by_hour[static_cast<std::size_t>(hour)];
  ++s.by_weekday[static_cast<std::size_t>(weekday)];
  ++s.heatmap[static_cast<std::size_t>(weekday)][static_cast<std::size_t>(hour)];
  if (weekday >= 5) ++s.weekend_commits;
  if (weekday == 4 && hour >= 16) ++s.friday_evening_commits;

  // Files and line counts.
  std::int64_t added = 0;
  std::int64_t deleted = 0;
  for (const FileChange& change : commit.files) {
    added += change.added;
    deleted += change.deleted;
    if (change.binary) ++s.binary_changes;
    FileAgg& agg = files_[change.path];
    ++agg.commits;
    agg.added += change.added;
    agg.deleted += change.deleted;
    const std::string extension = file_extension(change.path);
    ++extensions_[extension];
    extension_lines_[extension] += change.added + change.deleted;
  }
  s.added += added;
  s.deleted += deleted;

  if (!commit.files.empty()) {
    const std::int64_t size = added + deleted;
    const std::int64_t best = s.biggest_commit.added + s.biggest_commit.deleted;
    if (!s.has_biggest_commit || size > best ||
        (size == best && commit.hash < s.biggest_commit.hash)) {
      s.biggest_commit = make_ref(commit, added, deleted);
      s.has_biggest_commit = true;
    }
  }

  if (hour < kNightEndHour) {
    ++s.night_commits;
    s.night_examples.push_back(make_ref(commit, added, deleted));
    std::sort(s.night_examples.begin(), s.night_examples.end(),
              [](const CommitRef& a, const CommitRef& b) {
                return a.time != b.time ? a.time > b.time : a.hash < b.hash;
              });
    if (s.night_examples.size() > kNightExamples) s.night_examples.resize(kNightExamples);
  }

  // Authors: the e-mail address identifies a person, the newest name wins.
  std::string key = lower_utf8(commit.author_email);
  if (key.empty()) key = "name:" + commit.author_name;
  AuthorStat& author = authors_[key];
  if (author.commits == 0) {
    author.email = commit.author_email;
    author.name = commit.author_name;
    author.first_time = author.last_time = commit.time;
  } else {
    if (commit.time >= author.last_time && !commit.author_name.empty()) {
      author.name = commit.author_name;
    }
    author.first_time = std::min(author.first_time, commit.time);
    author.last_time = std::max(author.last_time, commit.time);
  }
  ++author.commits;
  author.added += added;
  author.deleted += deleted;

  // Messages. Auto-generated merge subjects would drown out everything else.
  if (!commit.is_merge()) {
    ++s.messages;
    subject_length_total_ += static_cast<std::int64_t>(codepoint_count(commit.subject));
    for (const std::string& word : tokenize_words(commit.subject)) ++words_[word];
    if (is_lazy_message(commit.subject)) {
      ++s.lazy_messages;
      std::string label = normalize_message(commit.subject);
      if (label.empty()) label = "(empty)";
      ++lazy_[label];
    }
    const std::string type = conventional_type(commit.subject);
    if (!type.empty()) {
      ++s.conventional_commits;
      ++types_[type];
    }
    const std::string lowered = lower_utf8(commit.subject);
    if (lowered.compare(0, 6, "revert") == 0) ++s.reverts;
  }
}

Stats Analyzer::finish() {
  Stats& s = stats_;
  s.active_days = static_cast<std::int64_t>(s.daily.size());
  s.author_count = static_cast<std::int64_t>(authors_.size());
  s.files_touched = static_cast<std::int64_t>(files_.size());
  if (s.messages > 0) {
    s.avg_subject_length =
        static_cast<double>(subject_length_total_) / static_cast<double>(s.messages);
  }

  // Longest silence between two consecutive commits.
  std::sort(times_.begin(), times_.end());
  for (std::size_t i = 1; i < times_.size(); ++i) {
    const std::int64_t gap = times_[i] - times_[i - 1];
    if (gap > s.longest_gap_seconds) {
      s.longest_gap_seconds = gap;
      s.gap_from = times_[i - 1];
      s.gap_to = times_[i];
    }
  }

  // Busiest day / week / month and the longest run of consecutive days.
  std::map<std::int64_t, std::int64_t> weeks;
  std::map<std::int64_t, std::int64_t> months;
  std::int64_t run = 0;
  std::int64_t run_start = 0;
  std::int64_t previous = 0;
  bool first = true;
  for (const auto& [day, count] : s.daily) {
    if (count > s.busiest_day_commits) {
      s.busiest_day_commits = count;
      s.busiest_day = day;
    }
    weeks[day - weekday_from_days(day)] += count;
    const Civil civil = civil_from_days(day);
    months[days_from_civil(civil.year, civil.month, 1)] += count;

    if (first || day != previous + 1) {
      run = 0;
      run_start = day;
    }
    ++run;
    if (run > s.longest_streak) {
      s.longest_streak = run;
      s.streak_start = run_start;
      s.streak_end = day;
    }
    previous = day;
    first = false;
  }
  for (const auto& [week, count] : weeks) {
    if (count > s.busiest_week_commits) {
      s.busiest_week_commits = count;
      s.busiest_week_start = week;
    }
  }
  for (const auto& [month, count] : months) {
    if (count > s.busiest_month_commits) {
      s.busiest_month_commits = count;
      s.busiest_month_day = month;
    }
  }

  // One person often commits from several addresses; the same display name is
  // the best signal we have without a .mailmap, so fold those together.
  std::vector<AuthorStat> by_email;
  by_email.reserve(authors_.size());
  for (auto& entry : authors_) by_email.push_back(std::move(entry.second));
  std::sort(by_email.begin(), by_email.end(), [](const AuthorStat& a, const AuthorStat& b) {
    if (a.commits != b.commits) return a.commits > b.commits;
    return a.email < b.email;
  });
  std::unordered_map<std::string, std::size_t> by_name;
  for (AuthorStat& a : by_email) {
    const std::string name = lower_utf8(trim(a.name));
    const auto found = name.empty() ? by_name.end() : by_name.find(name);
    if (found == by_name.end()) {
      if (!name.empty()) by_name[name] = s.authors.size();
      s.authors.push_back(std::move(a));
      continue;
    }
    AuthorStat& into = s.authors[found->second];
    into.commits += a.commits;
    into.added += a.added;
    into.deleted += a.deleted;
    into.first_time = std::min(into.first_time, a.first_time);
    into.last_time = std::max(into.last_time, a.last_time);
  }
  s.author_count = static_cast<std::int64_t>(s.authors.size());
  std::sort(s.authors.begin(), s.authors.end(), [](const AuthorStat& a, const AuthorStat& b) {
    if (a.commits != b.commits) return a.commits > b.commits;
    if (a.name != b.name) return a.name < b.name;
    return a.email < b.email;
  });

  std::vector<FileStat> files;
  files.reserve(files_.size());
  for (const auto& [path, agg] : files_) {
    files.push_back({path, agg.commits, agg.added, agg.deleted});
  }
  std::sort(files.begin(), files.end(), [](const FileStat& a, const FileStat& b) {
    if (a.commits != b.commits) return a.commits > b.commits;
    const std::int64_t churn_a = a.added + a.deleted;
    const std::int64_t churn_b = b.added + b.deleted;
    if (churn_a != churn_b) return churn_a > churn_b;
    return a.path < b.path;
  });
  if (files.size() > top_n_) files.resize(top_n_);
  s.top_files = std::move(files);

  s.top_extensions = top_counts(extensions_, top_n_);

  std::map<std::string, LanguageStat> languages;
  for (const auto& [extension, changes] : extensions_) {
    LanguageStat lang = language_for_extension(extension);
    LanguageStat& slot = languages.emplace(lang.name, lang).first->second;
    slot.changes += changes;
    const auto lines = extension_lines_.find(extension);
    if (lines != extension_lines_.end()) slot.lines += lines->second;
  }
  LanguageStat other = language_for_extension("");
  std::vector<LanguageStat> ranked;
  for (auto& entry : languages) {
    if (entry.first == other.name) {
      other.lines += entry.second.lines;
      other.changes += entry.second.changes;
    } else {
      ranked.push_back(std::move(entry.second));
    }
  }
  std::sort(ranked.begin(), ranked.end(), [](const LanguageStat& a, const LanguageStat& b) {
    if (a.lines != b.lines) return a.lines > b.lines;
    if (a.changes != b.changes) return a.changes > b.changes;
    return a.name < b.name;
  });
  while (ranked.size() > kLanguageSlots) {
    other.lines += ranked.back().lines;
    other.changes += ranked.back().changes;
    ranked.pop_back();
  }
  if (other.changes > 0) ranked.push_back(std::move(other));
  s.languages = std::move(ranked);

  s.top_words = top_counts(words_, top_n_);
  s.commit_types = top_counts(types_, top_n_);
  s.lazy_top = top_counts(lazy_, 3);

  Stats done = std::move(stats_);
  stats_ = Stats{};
  times_.clear();
  authors_.clear();
  files_.clear();
  extensions_.clear();
  extension_lines_.clear();
  words_.clear();
  lazy_.clear();
  types_.clear();
  subject_length_total_ = 0;
  return done;
}

Timeline bucket_timeline(const Stats& stats, std::size_t max_columns) {
  Timeline timeline;
  if (stats.commits == 0 || max_columns == 0) return timeline;
  const std::int64_t days = stats.span_days();
  const auto columns = static_cast<std::int64_t>(max_columns);
  timeline.days_per_bucket = (days + columns - 1) / columns;
  if (timeline.days_per_bucket < 1) timeline.days_per_bucket = 1;
  const std::int64_t count = (days + timeline.days_per_bucket - 1) / timeline.days_per_bucket;
  timeline.buckets.assign(static_cast<std::size_t>(count), 0);
  for (const auto& [day, commits] : stats.daily) {
    const std::int64_t index = (day - stats.first_day) / timeline.days_per_bucket;
    timeline.buckets[static_cast<std::size_t>(index)] += commits;
  }
  return timeline;
}

}  // namespace gitstory
