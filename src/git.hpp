// Talks to the `git` executable and turns `git log` output into Commit values.
#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gitstory {

struct FileChange {
  std::string path;
  std::int64_t added = 0;
  std::int64_t deleted = 0;
  bool binary = false;
};

struct Commit {
  std::string hash;
  std::string author_name;
  std::string author_email;
  std::string subject;
  std::int64_t time = 0;  // author time, seconds since the Unix epoch (UTC)
  int tz_offset = 0;      // author's UTC offset in seconds
  int parents = 0;
  std::vector<FileChange> files;

  bool is_merge() const { return parents > 1; }
  // Seconds since the epoch shifted into the author's own time zone.
  std::int64_t local_time() const { return time + tz_offset; }
};

struct LogOptions {
  std::string repo = ".";
  std::string rev;      // empty = HEAD
  bool all = false;     // every ref instead of a single revision
  std::string since;
  std::string until;
  std::string author;
  bool no_merges = false;
  std::int64_t max_count = 0;  // 0 = unlimited
};

struct RepoInfo {
  std::string name;  // directory name of the repository
  std::string root;  // work tree (or git dir for bare repositories)
};

class GitError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

using CommitSink = std::function<void(Commit&&)>;

// Incremental parser for the log format produced by build_log_command().
// Feed it output in arbitrarily sized chunks; it emits each commit once it is
// complete, so memory use stays flat even on very large histories.
class LogParser {
 public:
  explicit LogParser(CommitSink sink);
  void feed(std::string_view chunk);
  void finish();

 private:
  void line(std::string_view text);
  void flush();

  CommitSink sink_;
  std::string pending_;
  Commit current_;
  bool have_current_ = false;
};

std::vector<std::string> build_log_command(const LogOptions& options);

// "1700000000 +0300" -> time and offset in seconds.
bool parse_raw_date(std::string_view text, std::int64_t& time, int& tz_offset);

// Undoes git's C-style quoting of unusual path names ("a\tb\303\244").
std::string unquote_path(std::string_view text);

// Throws GitError when git is missing or `path` is not inside a repository.
RepoInfo describe_repo(const std::string& path);

// Streams the history selected by `options` into `sink`. An empty repository
// simply yields no commits. Throws GitError on any other failure.
void read_history(const LogOptions& options, const CommitSink& sink);

}  // namespace gitstory
