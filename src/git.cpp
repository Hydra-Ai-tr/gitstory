#include "git.hpp"

#include <charconv>
#include <utility>

#include "process.hpp"
#include "util.hpp"

namespace gitstory {

namespace {

constexpr char kRecord = '\x1e';  // starts every commit header
constexpr char kField = '\x1f';   // separates header fields

// hash, parents, author name, author email, author date (raw), subject.
// %aN/%aE respect .mailmap, so one person with several addresses is merged.
constexpr const char* kPretty = "--pretty=format:%x1e%H%x1f%P%x1f%aN%x1f%aE%x1f%ad%x1f%s";

bool parse_int(std::string_view text, std::int64_t& value) {
  if (text.empty()) return false;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto res = std::from_chars(begin, end, value);
  return res.ec == std::errc() && res.ptr == end;
}

std::string base_name(std::string_view path) {
  while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) path.remove_suffix(1);
  const std::size_t slash = path.find_last_of("/\\");
  if (slash != std::string_view::npos) path.remove_prefix(slash + 1);
  return std::string(path);
}

[[noreturn]] void throw_launch_failure(const ProcessResult& result) {
  throw GitError("could not run git (" + result.launch_error +
                 "). Is git installed and on your PATH?");
}

std::string first_line(std::string_view text) {
  const std::size_t nl = text.find('\n');
  return sanitize_text(nl == std::string_view::npos ? text : text.substr(0, nl));
}

std::string clean_error(std::string_view text) {
  std::string message = sanitize_text(text);
  for (const char* prefix : {"fatal: ", "error: "}) {
    const std::string_view p(prefix);
    if (message.compare(0, p.size(), p) == 0) {
      message.erase(0, p.size());
      break;
    }
  }
  return message.empty() ? "git failed without an error message" : message;
}

}  // namespace

// ---- LogParser -------------------------------------------------------------

LogParser::LogParser(CommitSink sink) : sink_(std::move(sink)) {}

void LogParser::feed(std::string_view chunk) {
  std::size_t start = 0;
  while (true) {
    const std::size_t nl = chunk.find('\n', start);
    if (nl == std::string_view::npos) {
      pending_.append(chunk.substr(start));
      return;
    }
    if (pending_.empty()) {
      line(chunk.substr(start, nl - start));
    } else {
      pending_.append(chunk.substr(start, nl - start));
      std::string complete;
      complete.swap(pending_);
      line(complete);
    }
    start = nl + 1;
  }
}

void LogParser::finish() {
  if (!pending_.empty()) {
    std::string complete;
    complete.swap(pending_);
    line(complete);
  }
  flush();
}

void LogParser::flush() {
  if (!have_current_) return;
  have_current_ = false;
  Commit done = std::move(current_);
  current_ = Commit{};
  sink_(std::move(done));
}

void LogParser::line(std::string_view text) {
  if (!text.empty() && text.back() == '\r') text.remove_suffix(1);
  if (text.empty()) return;

  if (text.front() == kRecord) {
    flush();
    const std::vector<std::string_view> fields = split(text.substr(1), kField, 6);
    if (fields.size() != 6) return;  // malformed header: skip the whole record
    Commit commit;
    if (!parse_raw_date(fields[4], commit.time, commit.tz_offset)) return;
    commit.hash = std::string(fields[0]);
    for (std::string_view parent : split(fields[1], ' ')) {
      if (!parent.empty()) ++commit.parents;
    }
    commit.author_name = sanitize_text(fields[2]);
    commit.author_email = sanitize_text(fields[3]);
    commit.subject = sanitize_text(fields[5]);
    current_ = std::move(commit);
    have_current_ = true;
    return;
  }

  if (!have_current_) return;
  // numstat: "<added>\t<deleted>\t<path>", with "-" for binary files.
  const std::size_t tab1 = text.find('\t');
  if (tab1 == std::string_view::npos) return;
  const std::size_t tab2 = text.find('\t', tab1 + 1);
  if (tab2 == std::string_view::npos) return;
  const std::string_view added = text.substr(0, tab1);
  const std::string_view deleted = text.substr(tab1 + 1, tab2 - tab1 - 1);
  const std::string_view path = text.substr(tab2 + 1);

  FileChange change;
  if (added == "-" && deleted == "-") {
    change.binary = true;
  } else if (!parse_int(added, change.added) || !parse_int(deleted, change.deleted)) {
    return;
  }
  change.path = sanitize_text(unquote_path(path));
  if (change.path.empty()) return;
  current_.files.push_back(std::move(change));
}

// ---- helpers ---------------------------------------------------------------

bool parse_raw_date(std::string_view text, std::int64_t& time, int& tz_offset) {
  const std::size_t space = text.find(' ');
  const std::string_view seconds = text.substr(0, space);
  if (!parse_int(seconds, time)) return false;
  tz_offset = 0;
  if (space == std::string_view::npos) return true;
  const std::string_view zone = text.substr(space + 1);
  if (zone.size() != 5 || (zone[0] != '+' && zone[0] != '-')) return true;
  for (std::size_t i = 1; i < 5; ++i) {
    if (zone[i] < '0' || zone[i] > '9') return true;
  }
  const int hours = (zone[1] - '0') * 10 + (zone[2] - '0');
  const int minutes = (zone[3] - '0') * 10 + (zone[4] - '0');
  const int offset = hours * 3600 + minutes * 60;
  // Real offsets stay within a day; ignore garbage from broken commits.
  if (offset <= 24 * 3600) tz_offset = zone[0] == '-' ? -offset : offset;
  return true;
}

std::string unquote_path(std::string_view text) {
  if (text.size() < 2 || text.front() != '"' || text.back() != '"') return std::string(text);
  text = text.substr(1, text.size() - 2);
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c != '\\' || i + 1 >= text.size()) {
      out += c;
      continue;
    }
    const char next = text[++i];
    switch (next) {
      case 'a': out += '\a'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      case 'v': out += '\v'; break;
      case '"': out += '"'; break;
      case '\\': out += '\\'; break;
      default:
        if (next >= '0' && next <= '7') {
          int value = next - '0';
          int digits = 1;
          while (digits < 3 && i + 1 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '7') {
            value = value * 8 + (text[++i] - '0');
            ++digits;
          }
          out += static_cast<char>(value & 0xFF);
        } else {
          out += next;
        }
    }
  }
  return out;
}

std::vector<std::string> build_log_command(const LogOptions& options) {
  std::vector<std::string> cmd = {
      "git",
      "-C", options.repo,
      // Pin down everything a user's config could change about the output.
      "-c", "core.quotePath=false",
      "-c", "color.ui=never",
      "-c", "i18n.logOutputEncoding=UTF-8",
      "log",
      "--no-show-signature",
      "--no-color",
      "--no-ext-diff",
      "--no-renames",
      "--numstat",
      "--date=raw",
      kPretty,
  };
  if (options.no_merges) cmd.push_back("--no-merges");
  // Values are glued to their option with '=', so they can never be read as
  // options themselves, whatever the user typed.
  if (!options.since.empty()) cmd.push_back("--since=" + options.since);
  if (!options.until.empty()) cmd.push_back("--until=" + options.until);
  if (!options.author.empty()) cmd.push_back("--author=" + options.author);
  if (options.max_count > 0) cmd.push_back("--max-count=" + std::to_string(options.max_count));
  if (options.all) {
    cmd.push_back("--all");
  } else if (!options.rev.empty()) {
    cmd.push_back("--end-of-options");
    cmd.push_back(options.rev);
  }
  cmd.push_back("--");
  return cmd;
}

RepoInfo describe_repo(const std::string& path) {
  std::string git_dir;
  ProcessResult result =
      run_process({"git", "-C", path, "rev-parse", "--absolute-git-dir"}, git_dir);
  if (!result.started) throw_launch_failure(result);
  if (result.exit_code != 0) {
    const std::string message = clean_error(result.error_output);
    if (message.find("not a git repository") != std::string::npos) {
      throw GitError("'" + path + "' is not inside a git repository");
    }
    throw GitError(message);
  }

  RepoInfo info;
  std::string top_level;
  result = run_process({"git", "-C", path, "rev-parse", "--show-toplevel"}, top_level);
  if (result.started && result.exit_code == 0 && !trim(top_level).empty()) {
    info.root = first_line(top_level);
    info.name = base_name(info.root);
  } else {
    // Bare repository: name it after its directory, minus the ".git" suffix.
    info.root = first_line(git_dir);
    info.name = base_name(info.root);
    const std::string_view suffix = ".git";
    if (info.name.size() > suffix.size() &&
        info.name.compare(info.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
      info.name.erase(info.name.size() - suffix.size());
    }
  }
  if (info.name.empty()) info.name = "repository";
  return info;
}

void read_history(const LogOptions& options, const CommitSink& sink) {
  LogParser parser(sink);
  const ProcessResult result = run_process(
      build_log_command(options), [&parser](std::string_view chunk) { parser.feed(chunk); });
  if (!result.started) throw_launch_failure(result);
  if (result.exit_code != 0) {
    // A freshly initialised repository has no HEAD yet; that is not an error.
    if (result.error_output.find("does not have any commits yet") != std::string::npos) return;
    throw GitError(clean_error(result.error_output));
  }
  parser.finish();
}

}  // namespace gitstory
