#include "analyze.hpp"

#include "util.hpp"

namespace gitstory {

namespace {

double ratio(std::int64_t part, std::int64_t whole) {
  return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0.0;
}

std::string plural(std::int64_t n, const char* singular, const char* many) {
  return format_number(n) + " " + (n == 1 ? singular : many);
}

}  // namespace

// Every line is backed by a threshold on a real number, so a tidy repository
// gets a short roast and a chaotic one gets a long one.
std::vector<std::string> build_roast(const Stats& s) {
  std::vector<std::string> lines;
  if (s.commits == 0) return lines;

  const double night = ratio(s.night_commits, s.commits);
  if (s.night_commits >= 3 && night >= 0.10) {
    lines.push_back(format_percent(night) +
                    " of commits landed between midnight and 5am. Sleep is also a feature.");
  } else if (s.night_commits == 0 && s.commits >= 50) {
    lines.push_back("Not a single commit between midnight and 5am. Suspiciously healthy.");
  }

  const double lazy = ratio(s.lazy_messages, s.messages);
  if (s.lazy_messages >= 3 && lazy >= 0.05) {
    std::string line = plural(s.lazy_messages, "commit message", "commit messages") +
                       " say nothing at all";
    if (!s.lazy_top.empty()) {
      line += " (\"" + s.lazy_top.front().label + "\" alone appears " +
              plural(s.lazy_top.front().count, "time", "times") + ")";
    }
    lines.push_back(line + ". Future you will be thrilled.");
  }

  if (s.messages >= 10 && s.avg_subject_length < 15.0) {
    lines.push_back("The average commit message is " +
                    std::to_string(static_cast<int>(s.avg_subject_length + 0.5)) +
                    " characters long. Haiku have more detail.");
  }

  if (s.longest_gap_seconds >= 30 * 86400) {
    lines.push_back("Went silent for " + format_duration(s.longest_gap_seconds) + " starting " +
                    format_date(floor_div(s.gap_from, 86400)) + ". The repo waited patiently.");
  }

  if (s.has_biggest_commit) {
    const std::int64_t size = s.biggest_commit.added + s.biggest_commit.deleted;
    if (size >= 2000) {
      lines.push_back("One commit changed " + format_number(size) +
                      " lines. \"Small refactor\", surely.");
    }
  }

  const double weekend = ratio(s.weekend_commits, s.commits);
  if (s.commits >= 10 && weekend >= 0.30) {
    lines.push_back(format_percent(weekend) +
                    " of commits happened on a weekend. Hobbies are overrated anyway.");
  }

  if (s.friday_evening_commits >= 3) {
    lines.push_back(plural(s.friday_evening_commits, "commit", "commits") +
                    " went in on a Friday after 4pm. Bold.");
  }

  if (!s.top_files.empty() && s.commits >= 20) {
    const FileStat& hot = s.top_files.front();
    const double share = ratio(hot.commits, s.commits);
    if (share >= 0.25) {
      lines.push_back(hot.path + " was touched in " + format_percent(share) +
                      " of all commits. It has seen things.");
    }
  }

  if (!s.authors.empty() && s.commits >= 20) {
    const AuthorStat& top = s.authors.front();
    const double share = ratio(top.commits, s.commits);
    if (s.author_count == 1) {
      lines.push_back("One author, " + plural(s.commits, "commit", "commits") +
                      ". A one-person band with a bus factor of exactly 1.");
    } else if (share >= 0.80) {
      lines.push_back(top.name + " wrote " + format_percent(share) +
                      " of the commits. Everyone else is here for moral support.");
    }
  }

  if (s.reverts >= 3) {
    lines.push_back(plural(s.reverts, "revert", "reverts") +
                    ". Two steps forward, one `git revert` back.");
  }

  if (s.longest_streak >= 14) {
    lines.push_back("A " + format_number(s.longest_streak) +
                    "-day commit streak. Someone please check on this person.");
  }

  if (s.messages >= 20 && ratio(s.conventional_commits, s.messages) >= 0.90) {
    lines.push_back("Over 90% Conventional Commits. The linter is proud; the rest of us are "
                    "a little scared.");
  }

  if (lines.empty()) {
    lines.push_back("Nothing to roast. Either this repo is flawless or it is too young to "
                    "have made mistakes yet.");
  }
  return lines;
}

}  // namespace gitstory
