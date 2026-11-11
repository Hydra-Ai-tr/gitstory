// Turns a stream of commits into the statistics every renderer works from.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "git.hpp"

namespace gitstory {

struct AuthorStat {
  std::string name;
  std::string email;
  std::int64_t commits = 0;
  std::int64_t added = 0;
  std::int64_t deleted = 0;
  std::int64_t first_time = 0;
  std::int64_t last_time = 0;
};

struct FileStat {
  std::string path;
  std::int64_t commits = 0;
  std::int64_t added = 0;
  std::int64_t deleted = 0;
};

struct CountItem {
  std::string label;
  std::int64_t count = 0;
};

struct LanguageStat {
  std::string name;
  std::int64_t lines = 0;    // lines added + deleted
  std::int64_t changes = 0;  // file changes
  int ansi = 245;            // xterm-256 colour
  std::string hex = "#8b949e";
};

// A single commit worth pointing at (biggest commit, a 3am commit, ...).
struct CommitRef {
  std::string hash;
  std::string subject;
  std::string author;
  std::int64_t time = 0;
  int tz_offset = 0;
  std::int64_t added = 0;
  std::int64_t deleted = 0;
};

struct Stats {
  // Totals
  std::int64_t commits = 0;
  std::int64_t merges = 0;
  std::int64_t added = 0;
  std::int64_t deleted = 0;
  std::int64_t files_touched = 0;   // distinct paths ever changed
  std::int64_t binary_changes = 0;
  std::int64_t author_count = 0;

  // Span. "Days" are day numbers since 1970-01-01 in each author's local time.
  std::int64_t first_time = 0;
  std::int64_t last_time = 0;
  std::int64_t first_day = 0;
  std::int64_t last_day = 0;
  std::int64_t active_days = 0;
  std::int64_t span_days() const { return commits == 0 ? 0 : last_day - first_day + 1; }

  // Rhythm
  std::array<std::int64_t, 24> by_hour{};
  std::array<std::int64_t, 7> by_weekday{};  // 0 = Monday
  std::array<std::array<std::int64_t, 24>, 7> heatmap{};
  std::int64_t night_commits = 0;           // 00:00-04:59 local time
  std::int64_t weekend_commits = 0;
  std::int64_t friday_evening_commits = 0;  // Friday from 16:00
  std::vector<CommitRef> night_examples;    // most recent first

  // Peaks
  std::int64_t busiest_day = 0;
  std::int64_t busiest_day_commits = 0;
  std::int64_t busiest_week_start = 0;  // the Monday of that week
  std::int64_t busiest_week_commits = 0;
  std::int64_t busiest_month_day = 0;   // first day of that month
  std::int64_t busiest_month_commits = 0;
  std::int64_t longest_streak = 0;      // consecutive days with a commit
  std::int64_t streak_start = 0;
  std::int64_t streak_end = 0;
  std::int64_t longest_gap_seconds = 0; // longest silence between two commits
  std::int64_t gap_from = 0;
  std::int64_t gap_to = 0;
  bool has_biggest_commit = false;
  CommitRef biggest_commit;

  // People, files, words
  std::vector<AuthorStat> authors;         // every author, most commits first
  std::vector<FileStat> top_files;
  std::vector<CountItem> top_extensions;
  std::vector<LanguageStat> languages;  // by lines changed, the tail folded into "Other"
  std::vector<CountItem> top_words;

  // Commit messages (merge commits are left out of these)
  std::int64_t messages = 0;
  double avg_subject_length = 0.0;
  std::int64_t lazy_messages = 0;
  std::vector<CountItem> lazy_top;
  std::int64_t conventional_commits = 0;
  std::vector<CountItem> commit_types;
  std::int64_t reverts = 0;

  // Commits per local day, for timelines.
  std::map<std::int64_t, std::int64_t> daily;
};

class Analyzer {
 public:
  explicit Analyzer(std::size_t top_n = 10);
  void add(const Commit& commit);
  Stats finish();

 private:
  struct FileAgg {
    std::int64_t commits = 0;
    std::int64_t added = 0;
    std::int64_t deleted = 0;
  };

  std::size_t top_n_;
  Stats stats_;
  std::vector<std::int64_t> times_;
  std::unordered_map<std::string, AuthorStat> authors_;
  std::unordered_map<std::string, FileAgg> files_;
  std::unordered_map<std::string, std::int64_t> extensions_;
  std::unordered_map<std::string, std::int64_t> extension_lines_;
  std::unordered_map<std::string, std::int64_t> words_;
  std::unordered_map<std::string, std::int64_t> lazy_;
  std::unordered_map<std::string, std::int64_t> types_;
  std::int64_t subject_length_total_ = 0;
};

// Splits the daily series into at most `max_columns` equal buckets.
struct Timeline {
  std::vector<std::int64_t> buckets;
  std::int64_t days_per_bucket = 1;
};
Timeline bucket_timeline(const Stats& stats, std::size_t max_columns);

// Lower-cased, de-noised words of a commit subject (no stop words, numbers
// or commit hashes).
std::vector<std::string> tokenize_words(std::string_view subject);

// "fix", "wip", "asdf", "." and friends.
bool is_lazy_message(std::string_view subject);
std::string normalize_message(std::string_view subject);

// "feat(ui)!: add x" -> "feat"; empty when not a Conventional Commit.
std::string conventional_type(std::string_view subject);

// "src/Main.CPP" -> ".cpp"; "Makefile" -> "(none)".
std::string file_extension(std::string_view path);

// ".cpp" -> C++ with its display colours; unknown extensions map to "Other".
LanguageStat language_for_extension(std::string_view extension);

// Light-hearted one-liners derived from the numbers.
std::vector<std::string> build_roast(const Stats& stats);

}  // namespace gitstory
