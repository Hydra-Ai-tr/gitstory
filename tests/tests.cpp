#include "analyze.hpp"
#include "cli.hpp"
#include "git.hpp"
#include "platform.hpp"
#include "process.hpp"
#include "render.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int g_failed = 0;
int g_passed = 0;

std::string show(const std::string& value) { return "\"" + value + "\""; }
std::string show(std::string_view value) { return show(std::string(value)); }
std::string show(std::int64_t value) { return std::to_string(value); }
std::string show(int value) { return std::to_string(value); }
std::string show(unsigned value) { return std::to_string(value); }
std::string show(std::size_t value) { return std::to_string(value); }
std::string show(char32_t value) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "U+%04X", static_cast<unsigned>(value));
  return buf;
}

void check(bool ok, const char* expr, const char* file, int line) {
  if (ok) {
    ++g_passed;
    return;
  }
  ++g_failed;
  std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
}

template <typename A, typename B>
void check_eq(const A& left, const B& right, const char* expr, const char* file, int line) {
  if (left == right) {
    ++g_passed;
    return;
  }
  ++g_failed;
  std::fprintf(stderr, "FAIL %s:%d: %s\n  left:  %s\n  right: %s\n", file, line, expr,
               show(left).c_str(), show(right).c_str());
}

#define CHECK(cond) check(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) check_eq((a), (b), #a " == " #b, __FILE__, __LINE__)

bool contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

// Undoes word wrapping so phrases can be found wherever the line broke.
std::string unwrap(std::string_view text) {
  std::string out;
  bool space = false;
  for (const char c : text) {
    if (c == ' ' || c == '\n') {
      space = true;
      continue;
    }
    if (space && !out.empty()) out += ' ';
    space = false;
    out += c;
  }
  return out;
}

gitstory::Commit make_commit(int year, unsigned month, unsigned day, int hour, int minute,
                             std::string hash, std::string name, std::string email,
                             std::string subject, int parents = 1) {
  gitstory::Commit commit;
  commit.hash = std::move(hash);
  commit.author_name = std::move(name);
  commit.author_email = std::move(email);
  commit.subject = std::move(subject);
  commit.parents = parents;
  const std::int64_t days = gitstory::days_from_civil(year, month, day);
  commit.time = days * 86400 + static_cast<std::int64_t>(hour) * 3600 +
                static_cast<std::int64_t>(minute) * 60;
  commit.tz_offset = 0;
  return commit;
}

void add_file(gitstory::Commit& commit, std::string path, std::int64_t added, std::int64_t deleted,
              bool binary = false) {
  gitstory::FileChange change;
  change.path = std::move(path);
  change.added = added;
  change.deleted = deleted;
  change.binary = binary;
  commit.files.push_back(std::move(change));
}

// ---- calendar and formatting ----------------------------------------------

void test_calendar() {
  using namespace gitstory;
  CHECK_EQ(days_from_civil(1970, 1, 1), 0);
  CHECK_EQ(weekday_from_days(0), 3);  // Thursday
  CHECK_EQ(weekday_from_days(days_from_civil(2024, 1, 1)), 0);  // Monday
  CHECK_EQ(std::string(weekday_name(0)), std::string("Mon"));
  CHECK_EQ(std::string(weekday_name(6)), std::string("Sun"));
  CHECK_EQ(std::string(weekday_name(-1)), std::string("???"));
  CHECK_EQ(std::string(month_name(1)), std::string("Jan"));
  CHECK_EQ(std::string(month_name(12)), std::string("Dec"));
  CHECK_EQ(std::string(month_name(0)), std::string("???"));

  const int years[] = {1600, 1900, 1970, 1999, 2000, 2024, 2025};
  const unsigned months[] = {1, 2, 3, 12};
  const unsigned days[] = {1, 28, 29, 31};
  for (const int year : years) {
    for (const unsigned month : months) {
      for (const unsigned day : days) {
        if (month == 2 && day > 29) continue;
        if ((month == 2 && day == 29) &&
            !((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
          continue;
        }
        if (day == 31 && (month == 2)) continue;
        const Civil back = civil_from_days(days_from_civil(year, month, day));
        CHECK_EQ(back.year, year);
        CHECK_EQ(back.month, month);
        CHECK_EQ(back.day, day);
      }
    }
  }

  const std::int64_t march = days_from_civil(2025, 3, 3);
  CHECK_EQ(format_date(march), std::string("Mar 3, 2025"));
  CHECK_EQ(format_month(march), std::string("Mar 2025"));
  CHECK_EQ(format_iso_date(march), std::string("2025-03-03"));
  CHECK_EQ(format_clock(3 * 3600 + 7 * 60), std::string("03:07"));
  CHECK_EQ(format_clock(0), std::string("00:00"));
  CHECK_EQ(format_clock(25 * 3600), std::string("01:00"));

  CHECK_EQ(floor_div(5, 2), 2);
  CHECK_EQ(floor_mod(5, 2), 1);
  CHECK_EQ(floor_div(-5, 2), -3);
  CHECK_EQ(floor_mod(-5, 2), 1);
  CHECK_EQ(floor_div(5, -2), -3);
  CHECK_EQ(floor_mod(5, -2), -1);

  CHECK_EQ(format_span_days(0), std::string("0 days"));
  CHECK_EQ(format_span_days(1), std::string("1 day"));
  CHECK_EQ(format_span_days(42), std::string("42 days"));
  CHECK_EQ(format_span_days(60), std::string("1 months"));
  CHECK_EQ(format_span_days(-4), std::string("0 days"));

  CHECK_EQ(format_duration(0), std::string("0m"));
  CHECK_EQ(format_duration(12 * 60), std::string("12m"));
  CHECK_EQ(format_duration(7 * 3600), std::string("7h"));
  CHECK_EQ(format_duration(86400), std::string("24h"));
  CHECK_EQ(format_duration(45 * 86400), std::string("45 days"));
  CHECK_EQ(format_duration(-8), std::string("0m"));
}

void test_numbers_and_text() {
  using namespace gitstory;
  CHECK_EQ(format_number(0), std::string("0"));
  CHECK_EQ(format_number(1234567), std::string("1,234,567"));
  CHECK_EQ(format_number(-1234), std::string("-1,234"));
  CHECK_EQ(format_number(INT64_MIN), std::string("-9,223,372,036,854,775,808"));
  CHECK_EQ(format_percent(0.0), std::string("0%"));
  CHECK_EQ(format_percent(0.123), std::string("12%"));
  CHECK_EQ(format_percent(0.004), std::string("<1%"));
  CHECK_EQ(format_percent(1.0), std::string("100%"));

  CHECK_EQ(trim(" \t hello \n"), std::string("hello"));
  CHECK_EQ(trim("   "), std::string(""));
  const auto parts = split("a:b:c", ':', 2);
  CHECK_EQ(parts.size(), 2u);
  CHECK_EQ(parts[0], std::string_view("a"));
  CHECK_EQ(parts[1], std::string_view("b:c"));
  const auto all = split("a,b,c", ',');
  CHECK_EQ(all.size(), 3u);
  CHECK_EQ(all[2], std::string_view("c"));

  CHECK_EQ(lower_utf8("AbC"), std::string("abc"));
  CHECK_EQ(lower_utf8("\u0130"), std::string("i"));  // Turkish İ
  CHECK_EQ(lower_utf8("\u00C4"), std::string("\u00E4"));
  CHECK_EQ(lower_utf8("\u0391"), std::string("\u03B1"));
  CHECK_EQ(lower_utf8("\u0410"), std::string("\u0430"));
  CHECK_EQ(to_lower(U'\u00D7'), U'\u00D7');  // multiplication sign stays

  std::size_t index = 0;
  char32_t cp = 0;
  CHECK(decode_utf8("A", index, cp));
  CHECK_EQ(cp, U'A');
  CHECK_EQ(index, 1u);

  index = 0;
  CHECK(decode_utf8("\u20AC", index, cp));
  CHECK_EQ(cp, U'\u20AC');
  CHECK_EQ(index, 3u);

  index = 0;
  CHECK(decode_utf8("\U0001F600", index, cp));
  CHECK_EQ(cp, U'\U0001F600');
  CHECK_EQ(index, 4u);

  const unsigned char overlong[] = {0xC0, 0x80};
  index = 0;
  CHECK(!decode_utf8(std::string_view(reinterpret_cast<const char*>(overlong), 2), index, cp));
  CHECK_EQ(cp, U'\uFFFD');
  CHECK_EQ(index, 1u);

  const unsigned char surrogate[] = {0xED, 0xA0, 0x80};
  index = 0;
  CHECK(!decode_utf8(std::string_view(reinterpret_cast<const char*>(surrogate), 3), index, cp));
  CHECK_EQ(index, 1u);

  CHECK_EQ(codepoint_count("\U0001F600!"), 2u);
  CHECK_EQ(display_width("ab"), 2u);
  CHECK_EQ(display_width("\u4E2D"), 2u);
  CHECK_EQ(display_width("e\u0301"), 1u);
  CHECK_EQ(display_width("\U0001F600"), 2u);

  CHECK_EQ(sanitize_text(" \thi\n"), std::string("hi"));
  CHECK_EQ(sanitize_text("a\x01" "b"), std::string("a b"));
  CHECK_EQ(sanitize_text("a\u202Eb"), std::string("ab"));
  CHECK_EQ(sanitize_text("\x1b[31mred"), std::string("[31mred"));

  CHECK_EQ(truncate_right("abcdef", 4, "\u2026"), std::string("abc\u2026"));
  CHECK_EQ(truncate_left("abcdef", 4, "\u2026"), std::string("\u2026def"));
  CHECK_EQ(truncate_right("ok", 4, "\u2026"), std::string("ok"));
  CHECK_EQ(truncate_right("\u4E2D\u4E2D", 3, "\u2026"), std::string("\u4E2D\u2026"));
  CHECK_EQ(pad_right("ab", 4), std::string("ab  "));
  CHECK_EQ(pad_left("ab", 4), std::string("  ab"));
  CHECK_EQ(pad_right("\u4E2D", 4), std::string("\u4E2D  "));
  CHECK_EQ(display_width(pad_right("\u4E2D", 4)), 4u);
}

// ---- git log parsing -------------------------------------------------------

void test_git_helpers() {
  using namespace gitstory;
  std::int64_t time = -1;
  int offset = 99;
  CHECK(parse_raw_date("1700000000 +0300", time, offset));
  CHECK_EQ(time, 1700000000);
  CHECK_EQ(offset, 3 * 3600);
  CHECK(parse_raw_date("1700000000 -0530", time, offset));
  CHECK_EQ(offset, -(5 * 3600 + 30 * 60));
  CHECK(parse_raw_date("1700000000", time, offset));
  CHECK_EQ(offset, 0);
  CHECK(parse_raw_date("100 +2400", time, offset));
  CHECK_EQ(offset, 24 * 3600);
  CHECK(parse_raw_date("100 +2401", time, offset));
  CHECK_EQ(offset, 0);
  CHECK(parse_raw_date("100 +03a0", time, offset));
  CHECK_EQ(offset, 0);
  CHECK(!parse_raw_date("nope", time, offset));
  CHECK(!parse_raw_date("", time, offset));

  CHECK_EQ(unquote_path("src/main.cpp"), std::string("src/main.cpp"));
  CHECK_EQ(unquote_path(R"("a\tb")"), std::string("a\tb"));
  CHECK_EQ(unquote_path(R"("a\\b")"), std::string("a\\b"));
  CHECK_EQ(unquote_path(R"("say \"hi\"")"), std::string("say \"hi\""));
  CHECK_EQ(unquote_path(R"("\101")"), std::string("A"));
  CHECK_EQ(unquote_path(R"("caf\303\244")"), std::string("caf\u00E4"));
  CHECK_EQ(unquote_path(R"("")"), std::string(""));
  CHECK_EQ(unquote_path("\"abc\\"), std::string("\"abc\\"));

  CHECK_EQ(quote_windows_arg(""), std::string("\"\""));
  CHECK_EQ(quote_windows_arg("hello"), std::string("hello"));
  CHECK_EQ(quote_windows_arg("hello world"), std::string("\"hello world\""));
  CHECK_EQ(quote_windows_arg("say \"hi\""), std::string("\"say \\\"hi\\\"\""));
  CHECK_EQ(quote_windows_arg("a b\\"), std::string("\"a b\\\\\""));
  CHECK_EQ(quote_windows_arg("a\\b"), std::string("a\\b"));
}

void test_log_parser() {
  using namespace gitstory;
  std::string log;
  log += '\x1e';
  log += "abc123";
  log += '\x1f';
  log += "p1 p2";
  log += '\x1f';
  log += "Ada Lovelace";
  log += '\x1f';
  log += "ada@example.com";
  log += '\x1f';
  log += "1700000000 +0000";
  log += '\x1f';
  log += "feat: add parser\r\n";
  log += "12\t3\tsrc/Main.CPP\n";
  log += "-\t-\tassets/logo.png\n";
  log += "\n";
  log += "not-a-stat\n";
  log += '\x1e';
  log += "def456";
  log += '\x1f';
  log += "";
  log += '\x1f';
  log += "Bob";
  log += '\x1f';
  log += "bob@example.com";
  log += '\x1f';
  log += "1700001000 -0500";
  log += '\x1f';
  log += "initial";  // no trailing newline; finish() must still emit it

  std::vector<Commit> commits;
  LogParser parser([&](Commit&& commit) { commits.push_back(std::move(commit)); });
  for (std::size_t i = 0; i < log.size(); i += 5) {
    parser.feed(std::string_view(log).substr(i, 5));
  }
  parser.finish();

  CHECK_EQ(commits.size(), 2u);
  if (commits.size() == 2) {
    CHECK_EQ(commits[0].hash, std::string("abc123"));
    CHECK_EQ(commits[0].parents, 2);
    CHECK(commits[0].is_merge());
    CHECK_EQ(commits[0].author_name, std::string("Ada Lovelace"));
    CHECK_EQ(commits[0].author_email, std::string("ada@example.com"));
    CHECK_EQ(commits[0].subject, std::string("feat: add parser"));
    CHECK_EQ(commits[0].time, 1700000000);
    CHECK_EQ(commits[0].tz_offset, 0);
    CHECK_EQ(commits[0].files.size(), 2u);
    if (commits[0].files.size() == 2) {
      CHECK_EQ(commits[0].files[0].path, std::string("src/Main.CPP"));
      CHECK_EQ(commits[0].files[0].added, 12);
      CHECK_EQ(commits[0].files[0].deleted, 3);
      CHECK(!commits[0].files[0].binary);
      CHECK(commits[0].files[1].binary);
      CHECK_EQ(commits[0].files[1].path, std::string("assets/logo.png"));
    }
    CHECK_EQ(commits[1].hash, std::string("def456"));
    CHECK_EQ(commits[1].parents, 0);
    CHECK(!commits[1].is_merge());
    CHECK_EQ(commits[1].subject, std::string("initial"));
    CHECK_EQ(commits[1].tz_offset, -5 * 3600);
    CHECK_EQ(commits[1].local_time(), commits[1].time + commits[1].tz_offset);
  }

  std::vector<Commit> skipped;
  LogParser bad([&](Commit&& commit) { skipped.push_back(std::move(commit)); });
  bad.feed("\x1eincomplete\n");
  bad.finish();
  CHECK_EQ(skipped.size(), 0u);
}

void test_log_command() {
  using namespace gitstory;
  LogOptions options;
  options.repo = "/work/repo";
  options.since = "2024-01-01";
  options.until = "2025-01-01";
  options.author = "ada";
  options.no_merges = true;
  options.max_count = 10;
  options.rev = "main";
  const std::vector<std::string> cmd = build_log_command(options);
  CHECK_EQ(cmd.front(), std::string("git"));
  CHECK_EQ(cmd[1], std::string("-C"));
  CHECK_EQ(cmd[2], std::string("/work/repo"));
  CHECK(contains(cmd[2], "repo"));
  auto has = [&](const std::string& flag) {
    return std::find(cmd.begin(), cmd.end(), flag) != cmd.end();
  };
  CHECK(has("--numstat"));
  CHECK(has("--date=raw"));
  CHECK(has("--no-merges"));
  CHECK(has("--since=2024-01-01"));
  CHECK(has("--until=2025-01-01"));
  CHECK(has("--author=ada"));
  CHECK(has("--max-count=10"));
  CHECK(has("--end-of-options"));
  CHECK(has("main"));
  CHECK_EQ(cmd.back(), std::string("--"));
  const auto end = std::find(cmd.begin(), cmd.end(), "--end-of-options");
  CHECK(end != cmd.end() && end + 1 != cmd.end() && *(end + 1) == "main");

  options.all = true;
  const std::vector<std::string> all = build_log_command(options);
  CHECK(std::find(all.begin(), all.end(), "--all") != all.end());
  CHECK(std::find(all.begin(), all.end(), "--end-of-options") == all.end());
  CHECK(std::find(all.begin(), all.end(), "main") == all.end());
}

// ---- analysis --------------------------------------------------------------

void test_analyze_helpers() {
  using namespace gitstory;
  CHECK_EQ(file_extension("src/Main.CPP"), std::string(".cpp"));
  CHECK_EQ(file_extension("Makefile"), std::string("(none)"));
  CHECK_EQ(file_extension(".gitignore"), std::string("(none)"));
  CHECK_EQ(file_extension("archive.tar.gz"), std::string(".gz"));
  CHECK_EQ(file_extension("dir.with.dots/file"), std::string("(none)"));
  CHECK_EQ(file_extension("a."), std::string("(none)"));

  CHECK_EQ(normalize_message("  Hello! "), std::string("hello"));
  CHECK(is_lazy_message("Fix."));
  CHECK(is_lazy_message("WIP"));
  CHECK(is_lazy_message("ok"));
  CHECK(is_lazy_message("..."));
  CHECK(is_lazy_message(""));
  CHECK(!is_lazy_message("feature"));

  CHECK_EQ(conventional_type("feat(ui)!: add x"), std::string("feat"));
  CHECK_EQ(conventional_type("Fix: typo"), std::string("fix"));
  CHECK_EQ(conventional_type("feat!: break"), std::string("feat"));
  CHECK_EQ(conventional_type("not conventional"), std::string(""));
  CHECK_EQ(conventional_type("foo: bar"), std::string(""));
  CHECK_EQ(conventional_type("feat(ui"), std::string(""));

  const auto words = tokenize_words("Don't add the abcdef1 button 12345");
  CHECK_EQ(words.size(), 3u);
  if (words.size() == 3) {
    CHECK_EQ(words[0], std::string("dont"));
    CHECK_EQ(words[1], std::string("add"));
    CHECK_EQ(words[2], std::string("button"));
  }
  const auto stopped = tokenize_words("\u0130\u00E7in fix");
  CHECK_EQ(stopped.size(), 1u);
  if (!stopped.empty()) CHECK_EQ(stopped[0], std::string("fix"));
}

void test_analyzer() {
  using namespace gitstory;
  Analyzer empty;
  const Stats blank = empty.finish();
  CHECK_EQ(blank.commits, 0);
  CHECK_EQ(blank.span_days(), 0);
  CHECK(bucket_timeline(blank, 10).buckets.empty());

  const std::int64_t monday = days_from_civil(2024, 1, 1);
  CHECK_EQ(weekday_from_days(monday), 0);

  Analyzer one(10);
  Commit commit = make_commit(2024, 1, 1, 3, 0, "aaa1111", "Ada", "ada@x.com",
                              "feat(ui): add button");
  add_file(commit, "src/Main.CPP", 20, 4);
  add_file(commit, "README", 1, 0);
  add_file(commit, "logo.bin", 0, 0, true);
  one.add(commit);
  const Stats stats = one.finish();

  CHECK_EQ(stats.commits, 1);
  CHECK_EQ(stats.merges, 0);
  CHECK_EQ(stats.added, 21);
  CHECK_EQ(stats.deleted, 4);
  CHECK_EQ(stats.binary_changes, 1);
  CHECK_EQ(stats.files_touched, 3);
  CHECK_EQ(stats.author_count, 1);
  CHECK_EQ(stats.night_commits, 1);
  CHECK_EQ(stats.weekend_commits, 0);
  CHECK_EQ(stats.friday_evening_commits, 0);
  CHECK_EQ(stats.by_hour[3], 1);
  CHECK_EQ(stats.by_weekday[0], 1);
  CHECK_EQ(stats.messages, 1);
  CHECK_EQ(stats.conventional_commits, 1);
  CHECK_EQ(stats.active_days, 1);
  CHECK_EQ(stats.longest_streak, 1);
  CHECK_EQ(stats.span_days(), 1);
  CHECK(stats.has_biggest_commit);
  CHECK_EQ(stats.biggest_commit.added + stats.biggest_commit.deleted, 25);
  CHECK_EQ(stats.first_day, monday);
  CHECK(!stats.top_extensions.empty());
  bool saw_cpp = false;
  bool saw_none = false;
  for (const CountItem& item : stats.top_extensions) {
    if (item.label == ".cpp") saw_cpp = true;
    if (item.label == "(none)") saw_none = true;
  }
  CHECK(saw_cpp);
  CHECK(saw_none);
  bool saw_button = false;
  for (const CountItem& item : stats.top_words) {
    if (item.label == "button") saw_button = true;
  }
  CHECK(saw_button);

  // finish() resets the analyzer.
  const Stats again = one.finish();
  CHECK_EQ(again.commits, 0);

  Analyzer people(1);
  Commit older = make_commit(2024, 1, 1, 10, 0, "bbb", "Old Name", "Ada@X.com", "rename later");
  add_file(older, "hot.cpp", 1, 0);
  add_file(older, "cold.cpp", 1, 0);
  Commit newer = make_commit(2024, 1, 2, 10, 0, "ccc", "Ada Lovelace", "ada@x.com",
                             "keep going");
  add_file(newer, "hot.cpp", 1, 0);
  Commit tie_big = make_commit(2024, 1, 3, 10, 0, "mmm", "Bob", "bob@x.com", "same size");
  add_file(tie_big, "other.cpp", 4, 0);
  Commit tie_small = make_commit(2024, 1, 3, 11, 0, "aaa", "Bob", "bob@x.com", "same size wins");
  add_file(tie_small, "other.cpp", 4, 0);
  people.add(older);
  people.add(newer);
  people.add(tie_big);
  people.add(tie_small);
  const Stats ranked = people.finish();
  CHECK_EQ(ranked.author_count, 2);
  CHECK_EQ(ranked.authors.size(), 2u);
  CHECK_EQ(ranked.authors[0].email, std::string("Ada@X.com"));
  CHECK_EQ(ranked.authors[0].name, std::string("Ada Lovelace"));
  CHECK_EQ(ranked.authors[0].commits, 2);
  CHECK_EQ(ranked.top_files.size(), 1u);
  if (!ranked.top_files.empty()) CHECK_EQ(ranked.top_files[0].path, std::string("other.cpp"));
  CHECK(ranked.has_biggest_commit);
  CHECK_EQ(ranked.biggest_commit.hash, std::string("aaa"));
  CHECK_EQ(ranked.longest_streak, 3);
  CHECK_EQ(ranked.busiest_day_commits, 2);
  CHECK_EQ(ranked.busiest_week_start, monday);
  CHECK_EQ(ranked.busiest_month_day, days_from_civil(2024, 1, 1));

  Analyzer aliases;
  aliases.add(make_commit(2024, 1, 1, 10, 0, "x1", "David Peter", "work@x.com", "one"));
  aliases.add(make_commit(2024, 1, 2, 10, 0, "x2", "David Peter", "home@x.com", "two"));
  aliases.add(make_commit(2024, 1, 3, 10, 0, "x3", "david peter ", "work@x.com", "three"));
  aliases.add(make_commit(2024, 1, 4, 10, 0, "x4", "", "anon@x.com", "four"));
  const Stats merged = aliases.finish();
  CHECK_EQ(merged.author_count, 2);
  if (merged.authors.size() == 2) {
    CHECK_EQ(merged.authors[0].commits, 3);
    CHECK_EQ(merged.authors[0].first_time, days_from_civil(2024, 1, 1) * 86400 + 36000);
    CHECK_EQ(merged.authors[1].email, std::string("anon@x.com"));
  }

  Analyzer streak;
  for (int day = 1; day <= 3; ++day) {
    streak.add(make_commit(2024, 1, day, 12, 0, "s" + std::to_string(day), "A", "a@x.com",
                           "working on the streak"));
  }
  streak.add(make_commit(2024, 1, 5, 12, 0, "s5", "A", "a@x.com", "after the gap"));
  const Stats run = streak.finish();
  CHECK_EQ(run.longest_streak, 3);
  CHECK_EQ(run.streak_start, monday);
  CHECK_EQ(run.streak_end, monday + 2);
  CHECK_EQ(run.active_days, 4);
  CHECK(run.longest_gap_seconds >= 86400);

  Analyzer nights;
  for (int i = 0; i < 4; ++i) {
    nights.add(make_commit(2024, 1, 1, i, 0, std::string(1, static_cast<char>('d' - i)), "A",
                           "a@x.com", "late night work"));
  }
  const Stats night = nights.finish();
  CHECK_EQ(night.night_commits, 4);
  CHECK_EQ(night.night_examples.size(), 3u);
  if (night.night_examples.size() == 3) {
    CHECK_EQ(night.night_examples[0].hash, std::string("a"));
    CHECK_EQ(night.night_examples[1].hash, std::string("b"));
    CHECK_EQ(night.night_examples[2].hash, std::string("c"));
  }

  Analyzer zoned;
  Commit late = make_commit(2024, 1, 1, 22, 0, "tz1", "A", "a@x.com", "still monday in utc");
  late.tz_offset = 3 * 3600;  // Monday 22:00 UTC is Tuesday 01:00 locally
  zoned.add(late);
  Commit early = make_commit(2024, 1, 7, 2, 0, "tz2", "A", "a@x.com", "sunday morning utc");
  early.tz_offset = -5 * 3600;  // Sunday 02:00 UTC is Saturday 21:00 locally
  zoned.add(early);
  const Stats zones = zoned.finish();
  CHECK_EQ(zones.night_commits, 1);
  CHECK_EQ(zones.weekend_commits, 1);
  CHECK_EQ(zones.by_hour[1], 1);
  CHECK_EQ(zones.by_hour[21], 1);
  CHECK_EQ(zones.by_weekday[1], 1);  // Tuesday
  CHECK_EQ(zones.by_weekday[5], 1);  // Saturday
}

void test_timeline_and_messages() {
  using namespace gitstory;
  Analyzer analyzer;
  Commit merge = make_commit(2024, 1, 1, 12, 0, "m1", "A", "a@x.com", "Merge branch 'feature'", 2);
  add_file(merge, "a.cpp", 1, 0);
  analyzer.add(merge);
  analyzer.add(make_commit(2024, 1, 1, 15, 0, "r1", "A", "a@x.com", "Revert the widget"));
  analyzer.add(make_commit(2024, 1, 10, 15, 0, "c1", "A", "a@x.com", "fix: the widget again"));
  const Stats stats = analyzer.finish();
  CHECK_EQ(stats.merges, 1);
  CHECK_EQ(stats.messages, 2);  // merge subjects are excluded
  CHECK_EQ(stats.reverts, 1);
  CHECK_EQ(stats.conventional_commits, 1);
  CHECK_EQ(stats.lazy_messages, 0);
  bool merge_word = false;
  for (const CountItem& item : stats.top_words) {
    if (item.label == "merge" || item.label == "branch") merge_word = true;
  }
  CHECK(!merge_word);

  const Timeline timeline = bucket_timeline(stats, 4);
  CHECK_EQ(stats.span_days(), 10);
  CHECK_EQ(timeline.days_per_bucket, 3);
  CHECK_EQ(timeline.buckets.size(), 4u);
  std::int64_t total = 0;
  for (const std::int64_t count : timeline.buckets) total += count;
  CHECK_EQ(total, stats.commits);
  CHECK(bucket_timeline(stats, 0).buckets.empty());
}

void test_roast() {
  using namespace gitstory;
  CHECK(build_roast(Stats{}).empty());

  Analyzer tidy;
  tidy.add(make_commit(2024, 1, 1, 12, 0, "t1", "Ada", "ada@x.com",
                       "Implement the parser carefully"));
  const std::vector<std::string> calm = build_roast(tidy.finish());
  CHECK_EQ(calm.size(), 1u);
  if (!calm.empty()) CHECK(contains(calm[0], "Nothing to roast"));

  Analyzer night;
  for (int i = 0; i < 3; ++i) {
    night.add(make_commit(2024, 1, 1, 2, i, "n" + std::to_string(i), "Ada", "ada@x.com",
                          "implement the parser carefully"));
  }
  const std::vector<std::string> owls = build_roast(night.finish());
  CHECK_EQ(owls.size(), 1u);
  if (!owls.empty()) CHECK(contains(owls[0], "midnight and 5am"));

  Analyzer healthy;
  for (int i = 0; i < 50; ++i) {
    const std::string author = i % 2 == 0 ? "ada@x.com" : "bob@x.com";
    Commit commit = make_commit(2024, 1, 1, 12, 0, "h" + std::to_string(i),
                                i % 2 == 0 ? "Ada" : "Bob", author,
                                "Document the public interface in detail");
    commit.time += i;
    add_file(commit, "file" + std::to_string(i) + ".cpp", 2, 1);
    healthy.add(commit);
  }
  const std::vector<std::string> fit = build_roast(healthy.finish());
  CHECK_EQ(fit.size(), 1u);
  if (!fit.empty()) CHECK(contains(fit[0], "Suspiciously healthy"));

  Analyzer lazy;
  for (int i = 0; i < 3; ++i) {
    lazy.add(make_commit(2024, 1, 1, 12, i, "l" + std::to_string(i), "Ada", "ada@x.com", "wip"));
  }
  for (int i = 0; i < 7; ++i) {
    lazy.add(make_commit(2024, 1, 1, 13, i, "g" + std::to_string(i), "Ada", "ada@x.com",
                         "Describe the change with a proper sentence"));
  }
  const std::vector<std::string> shrugs = build_roast(lazy.finish());
  bool lazy_line = false;
  for (const std::string& line : shrugs) {
    if (contains(line, "say nothing at all") && contains(line, "wip")) lazy_line = true;
  }
  CHECK(lazy_line);

  Analyzer huge;
  Commit big = make_commit(2024, 1, 1, 12, 0, "big", "Ada", "ada@x.com", "rewrite everything");
  add_file(big, "core.cpp", 2000, 0);
  huge.add(big);
  const std::vector<std::string> dump = build_roast(huge.finish());
  CHECK_EQ(dump.size(), 1u);
  if (!dump.empty()) CHECK(contains(dump[0], "2,000 lines"));

  Analyzer quiet;
  quiet.add(make_commit(2024, 1, 1, 12, 0, "q1", "Ada", "ada@x.com", "start of a long pause"));
  Commit later = make_commit(2024, 2, 10, 12, 0, "q2", "Ada", "ada@x.com", "back after the pause");
  quiet.add(later);
  bool silent = false;
  for (const std::string& line : build_roast(quiet.finish())) {
    if (contains(line, "Went silent")) silent = true;
  }
  CHECK(silent);

  Analyzer weekend;
  for (int i = 0; i < 10; ++i) {
    weekend.add(make_commit(2024, 1, 6, 12, i, "w" + std::to_string(i), "Ada", "ada@x.com",
                            "saturday work on the project"));
  }
  bool weekend_line = false;
  for (const std::string& line : build_roast(weekend.finish())) {
    if (contains(line, "weekend")) weekend_line = true;
  }
  CHECK(weekend_line);

  Analyzer friday;
  for (int i = 0; i < 3; ++i) {
    friday.add(make_commit(2024, 1, 5, 16, i, "f" + std::to_string(i), "Ada", "ada@x.com",
                           "ship it before the weekend"));
  }
  bool friday_line = false;
  for (const std::string& line : build_roast(friday.finish())) {
    if (contains(line, "Friday")) friday_line = true;
  }
  CHECK(friday_line);

  Analyzer streak;
  for (int day = 1; day <= 14; ++day) {
    streak.add(make_commit(2024, 1, day, 12, 0, "st" + std::to_string(day), "Ada", "ada@x.com",
                           "daily progress on the project"));
  }
  bool streak_line = false;
  for (const std::string& line : build_roast(streak.finish())) {
    if (contains(line, "14-day commit streak")) streak_line = true;
  }
  CHECK(streak_line);

  Analyzer conventional;
  for (int i = 0; i < 20; ++i) {
    const std::string email = i % 2 == 0 ? "ada@x.com" : "bob@x.com";
    Commit commit = make_commit(2024, 1, 1, 12, 0, "cv" + std::to_string(i), "Person", email,
                                "feat: something reasonably long here");
    commit.time += i;
    add_file(commit, "f" + std::to_string(i) + ".cpp", 1, 0);
    conventional.add(commit);
  }
  bool strict = false;
  for (const std::string& line : build_roast(conventional.finish())) {
    if (contains(line, "Conventional Commits")) strict = true;
  }
  CHECK(strict);

  Analyzer reverts;
  for (int i = 0; i < 3; ++i) {
    reverts.add(make_commit(2024, 1, 1, 12, i, "rv" + std::to_string(i), "Ada", "ada@x.com",
                            "Revert the thing"));
  }
  bool undo = false;
  for (const std::string& line : build_roast(reverts.finish())) {
    if (contains(line, "revert")) undo = true;
  }
  CHECK(undo);
}

// ---- command line ----------------------------------------------------------

void expect_usage(const std::vector<std::string>& args, const char* fragment) {
  bool threw = false;
  try {
    gitstory::parse_args(args);
  } catch (const gitstory::UsageError& error) {
    threw = true;
    CHECK(contains(error.what(), fragment));
  } catch (const std::exception& error) {
    threw = true;
    std::fprintf(stderr, "FAIL unexpected exception: %s\n", error.what());
    ++g_failed;
  }
  CHECK(threw);
}

void test_cli() {
  using namespace gitstory;
  const Options defaults = parse_args({});
  CHECK_EQ(defaults.log.repo, std::string("."));
  CHECK(!defaults.help);
  CHECK(!defaults.json);
  CHECK_EQ(defaults.top, 10u);
  CHECK(defaults.color == ColorMode::Auto);

  const Options help = parse_args({"-h"});
  CHECK(help.help);
  CHECK(parse_args({"--version"}).version);
  CHECK(contains(usage_text(), "gitstory " GITSTORY_VERSION));

  const Options full = parse_args({"repo", "--json", "--top", "3", "--width=72", "--color",
                                   "never", "--ascii", "--roast", "--no-merges", "--since",
                                   "2024-01-01", "--until", "2025-01-01", "--author", "ada", "-n",
                                   "10", "--rev", "main", "--svg", "card.svg", "--light"});
  CHECK_EQ(full.log.repo, std::string("repo"));
  CHECK(full.json);
  CHECK_EQ(full.top, 3u);
  CHECK_EQ(full.width, 72u);
  CHECK(full.color == ColorMode::Never);
  CHECK(full.glyphs == GlyphMode::Ascii);
  CHECK(full.roast);
  CHECK(full.log.no_merges);
  CHECK_EQ(full.log.since, std::string("2024-01-01"));
  CHECK_EQ(full.log.until, std::string("2025-01-01"));
  CHECK_EQ(full.log.author, std::string("ada"));
  CHECK_EQ(full.log.max_count, 10);
  CHECK_EQ(full.log.rev, std::string("main"));
  CHECK_EQ(full.svg_path, std::string("card.svg"));
  CHECK(full.light_theme);

  const Options positional = parse_args({"--", "--weird-path"});
  CHECK_EQ(positional.log.repo, std::string("--weird-path"));
  CHECK(parse_args({"--all"}).log.all);
  CHECK(parse_args({"--color=always"}).color == ColorMode::Always);
  CHECK_EQ(parse_args({"--svg", "-"}).svg_path, std::string("-"));
  CHECK_EQ(parse_args({"-"}).log.repo, std::string("-"));

  expect_usage({"--all", "--rev", "main"}, "--all and --rev");
  expect_usage({"--json", "--svg", "-"}, "standard output");
  expect_usage({"--color", "sometimes"}, "auto, always or never");
  CHECK(parse_args({}).glyphs == GlyphMode::Auto);
  CHECK(parse_args({"--glyphs", "basic"}).glyphs == GlyphMode::Basic);
  CHECK(parse_args({"--glyphs=full"}).glyphs == GlyphMode::Full);
  CHECK(parse_args({"--glyphs", "ascii"}).glyphs == GlyphMode::Ascii);
  expect_usage({"--glyphs", "fancy"}, "auto, full, basic or ascii");
  expect_usage({"--top", "0"}, "--top");
  expect_usage({"--top", "1001"}, "--top");
  expect_usage({"--nope"}, "unknown option");
  expect_usage({"--help=true"}, "does not take a value");
  expect_usage({"repo", "extra"}, "unexpected extra argument");
  expect_usage({""}, "must not be empty");
  expect_usage({"--rev", ""}, "non-empty");
  expect_usage({"--svg"}, "needs a value");
  CHECK_EQ(parse_args({"--top", "1000"}).top, 1000u);
}

// ---- rendering -------------------------------------------------------------

void test_render() {
  using namespace gitstory;
  CHECK_EQ(json_escape("a\"b\\c\n\t"), std::string("a\\\"b\\\\c\\n\\t"));
  CHECK_EQ(json_escape(std::string("\x01", 1)), std::string("\\u0001"));
  CHECK_EQ(xml_escape("a<b>&\"c'"), std::string("a&lt;b&gt;&amp;&quot;c&#39;"));

  RepoInfo repo;
  repo.name = "demo";
  repo.root = "/work/demo";
  const std::string empty_json = render_json(Stats{}, repo);
  CHECK(contains(empty_json, "\"gitstory_version\": \"0.1.0\""));
  CHECK(contains(empty_json, "\"repository\": \"demo\""));
  CHECK(!contains(empty_json, "\"span\""));
  CHECK(contains(empty_json, "\"average_length\": 0.0000"));

  Analyzer analyzer;
  Commit commit = make_commit(2024, 1, 1, 3, 15, "abc123", "Ada", "ada@x.com", "feat: widgets");
  add_file(commit, "src/app.cpp", 8, 1);
  analyzer.add(commit);
  const Stats stats = analyzer.finish();
  const std::string json = render_json(stats, repo);
  CHECK(contains(json, "\"commits\": 1"));
  CHECK(contains(json, "2024-01-01"));
  CHECK(contains(json, "03:15"));
  CHECK(contains(json, "ada@x.com"));
  CHECK(contains(json, "\"type\": \"feat\""));

  repo.name = "a&b";
  SvgOptions dark;
  const std::string svg = render_svg(stats, repo, dark);
  CHECK(contains(svg, "<svg "));
  CHECK(contains(svg, "</svg>"));
  CHECK(contains(svg, "a&amp;b"));
  CHECK(contains(svg, "#0d1117"));
  SvgOptions light;
  light.dark = false;
  CHECK(contains(render_svg(stats, repo, light), "#ffffff"));

  CHECK(contains(json, "\"languages\""));
  CHECK(contains(json, "\"name\": \"C++\""));
  CHECK(contains(svg, "C++ 100.0%"));

  repo.name = "demo";
  TermOptions term;
  term.color = false;
  term.glyphs = GlyphSet::Ascii;
  term.width = 80;
  term.top = 5;
  const std::string report = render_terminal(stats, repo, term);
  CHECK(contains(report, "demo"));
  CHECK(contains(report, "Activity"));
  CHECK(contains(report, "Rhythm"));
  CHECK(contains(report, "Hot files"));
  CHECK(contains(report, "Languages"));
  CHECK(contains(report, "came together in a single day"));
  CHECK(!contains(report, "\x1b["));
  bool pure_ascii = true;
  for (const char c : report) {
    if (static_cast<unsigned char>(c) >= 0x80) pure_ascii = false;
  }
  CHECK(pure_ascii);
  term.color = true;
  term.glyphs = GlyphSet::Full;
  term.roast = true;
  const std::string colored = render_terminal(stats, repo, term);
  CHECK(contains(colored, "\x1b["));
  CHECK(contains(colored, "Roast"));
  CHECK(render_terminal(Stats{}, repo, term) == "No commits found.\n");
}

bool has_eighth_blocks(std::string_view text) {
  for (const char* glyph : {"\u2581", "\u2582", "\u2583", "\u2585", "\u2586", "\u2587",
                            "\u258F", "\u258E", "\u258D", "\u258B", "\u258A", "\u2589",
                            "\u256D", "\u256E", "\u2570", "\u256F"}) {
    if (contains(text, glyph)) return true;
  }
  return false;
}

gitstory::Stats history(int days, int per_day_mod) {
  using namespace gitstory;
  Analyzer analyzer(5);
  const std::int64_t start = days_from_civil(2023, 3, 1);
  for (int d = 0; d < days; d += 1 + (d % per_day_mod)) {
    const Civil c = civil_from_days(start + d);
    Commit commit = make_commit(c.year, c.month, c.day, 9 + d % 12, 0, "h" + std::to_string(d),
                                d % 3 == 0 ? "Ada" : "Bob",
                                d % 3 == 0 ? "ada@x.com" : "bob@x.com",
                                "Improve the renderer for day " + std::to_string(d));
    add_file(commit, d % 2 == 0 ? "src/app.ts" : "web/index.html", 10 + d % 7, d % 5);
    add_file(commit, "docs/readme.md", 1, 0);
    analyzer.add(commit);
  }
  return analyzer.finish();
}

void test_terminal_layouts() {
  using namespace gitstory;
  RepoInfo repo;
  repo.name = "layouts";
  TermOptions term;
  term.width = 100;

  // Short history: one column per day with the day numbers underneath.
  const Stats week = history(9, 3);
  term.glyphs = GlyphSet::Full;
  const std::string daily = render_terminal(week, repo, term);
  CHECK(contains(daily, "Mar"));
  CHECK(contains(daily, "Wed"));
  CHECK(contains(daily, "TypeScript"));
  CHECK(contains(daily, "HTML"));
  CHECK(contains(unwrap(daily), "by Ada and Bob."));

  // A few months: GitHub-style calendar with a legend.
  const Stats months = history(120, 4);
  const std::string calendar = render_terminal(months, repo, term);
  CHECK(contains(calendar, "less "));
  CHECK(contains(calendar, "more"));

  // Years: bucketed columns.
  const Stats years = history(1500, 9);
  const std::string longer = render_terminal(years, repo, term);
  CHECK(contains(longer, "each column = "));
  CHECK(contains(longer, "2024"));

  // The classic console font has no eighth blocks or rounded corners.
  term.glyphs = GlyphSet::Basic;
  for (const Stats* s : {&week, &months, &years}) {
    const std::string basic = render_terminal(*s, repo, term);
    CHECK(!has_eighth_blocks(basic));
    CHECK(contains(basic, "\u250C"));
  }

  // Every line respects the requested width.
  for (const std::size_t width : {60u, 80u, 110u}) {
    term.width = width;
    term.glyphs = GlyphSet::Ascii;
    for (const Stats* s : {&week, &months, &years}) {
      const std::string text = render_terminal(*s, repo, term);
      std::size_t start = 0;
      std::size_t widest = 0;
      while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        widest = std::max(widest, display_width(std::string_view(text).substr(start, end - start)));
        start = end + 1;
      }
      CHECK(widest <= width);
    }
  }
}

void test_languages() {
  using namespace gitstory;
  CHECK_EQ(language_for_extension(".cpp").name, std::string("C++"));
  CHECK_EQ(language_for_extension(".tsx").name, std::string("TypeScript"));
  CHECK_EQ(language_for_extension(".png").name, std::string("Images"));
  CHECK_EQ(language_for_extension("(none)").name, std::string("Other"));
  CHECK_EQ(language_for_extension(".unknownext").name, std::string("Other"));
  CHECK_EQ(language_for_extension(".js").hex, std::string("#f1e05a"));

  Analyzer analyzer;
  Commit commit = make_commit(2024, 1, 1, 12, 0, "l1", "Ada", "ada@x.com", "polyglot");
  const char* paths[] = {"a.js", "b.ts", "c.py", "d.go", "e.rs", "f.rb", "g.java", "h.php",
                         "Makefile"};
  std::int64_t lines = 90;
  for (const char* path : paths) {
    add_file(commit, path, lines, 0);
    lines -= 10;
  }
  analyzer.add(commit);
  const Stats stats = analyzer.finish();
  CHECK_EQ(stats.languages.size(), 7u);
  if (stats.languages.size() == 7) {
    CHECK_EQ(stats.languages[0].name, std::string("JavaScript"));
    CHECK_EQ(stats.languages[0].lines, 90);
    CHECK_EQ(stats.languages[5].name, std::string("Ruby"));
    CHECK_EQ(stats.languages[6].name, std::string("Other"));
    CHECK_EQ(stats.languages[6].lines, 30 + 20 + 10);
    CHECK_EQ(stats.languages[6].changes, 3);
  }
}

// ---- processes and a real repository ---------------------------------------

// Git marks object files read-only, so a plain remove_all fails on Windows.
void remove_tree(const std::filesystem::path& root) {
  std::error_code error;
  if (!std::filesystem::exists(root, error)) return;
  const auto writable = std::filesystem::perms::owner_write;
  for (auto it = std::filesystem::recursive_directory_iterator(
           root, std::filesystem::directory_options::skip_permission_denied, error);
       !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
    std::filesystem::permissions(it->path(), writable, std::filesystem::perm_options::add, error);
  }
  std::filesystem::permissions(root, writable, std::filesystem::perm_options::add, error);
  std::filesystem::remove_all(root, error);
}

struct TempDir {
  std::filesystem::path path;
  explicit TempDir(std::string name) {
    path = std::filesystem::temp_directory_path() / name;
    remove_tree(path);
    std::filesystem::create_directories(path);
  }
  ~TempDir() { remove_tree(path); }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
};

gitstory::ProcessResult git(const std::vector<std::string>& args, std::string& output) {
  return gitstory::run_process(args, output);
}

void require_git(const std::vector<std::string>& args) {
  std::string output;
  const gitstory::ProcessResult result = git(args, output);
  if (!result.started || result.exit_code != 0) {
    throw std::runtime_error(args[0] + " failed: " + result.launch_error + result.error_output +
                             output);
  }
}

void test_process_and_repo() {
  using namespace gitstory;
  std::string output;
  const ProcessResult version = run_process({"git", "--version"}, output);
  CHECK(version.started);
  CHECK_EQ(version.exit_code, 0);
  CHECK(contains(output, "git version"));

  std::string ignored;
  const ProcessResult missing = run_process({"gitstory-missing-binary-xyz"}, ignored);
  CHECK(!missing.started);
  CHECK(!missing.launch_error.empty());

  const ProcessResult empty_argv = run_process({}, ignored);
  CHECK(!empty_argv.started);
  CHECK_EQ(empty_argv.launch_error, std::string("empty command"));

  std::string bad_out;
  const ProcessResult bad = run_process({"git", "not-a-real-subcommand-xyz"}, bad_out);
  CHECK(bad.started);
  CHECK(bad.exit_code != 0);
  CHECK(!bad.error_output.empty());

  bool threw = false;
  try {
    run_process({"git", "--version"}, [](std::string_view) { throw std::runtime_error("stop"); });
  } catch (const std::runtime_error& error) {
    threw = true;
    CHECK_EQ(std::string(error.what()), std::string("stop"));
  }
  CHECK(threw);

  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  TempDir temp("gitstory-tests-" + std::to_string(stamp));
  const std::string root = temp.path.string();
  std::string error;
  const std::filesystem::path readme = temp.path / "README.md";
  CHECK(write_file(readme.string(), "hello\n", error));
  CHECK(error.empty());

  bool not_repo = false;
  try {
    describe_repo(root);
  } catch (const GitError& failure) {
    not_repo = contains(failure.what(), "not inside a git repository");
  }
  CHECK(not_repo);

  require_git({"git", "init", root});
  std::vector<Commit> none;
  read_history(LogOptions{root, "", false, "", "", "", false, 0},
               [&](Commit&& commit) { none.push_back(std::move(commit)); });
  CHECK_EQ(none.size(), 0u);

  require_git({"git", "-C", root, "config", "core.autocrlf", "false"});
  require_git({"git", "-C", root, "add", "--", "README.md"});
  require_git({"git", "-C", root, "-c", "user.email=ada@example.com", "-c",
               "user.name=Ada Lovelace", "-c", "commit.gpgsign=false", "commit", "-m",
               "feat: first"});

  const RepoInfo info = describe_repo(root);
  CHECK_EQ(info.name, temp.path.filename().string());
  CHECK(contains(info.root, info.name));

  LogOptions options;
  options.repo = root;
  std::vector<Commit> commits;
  read_history(options, [&](Commit&& commit) { commits.push_back(std::move(commit)); });
  CHECK_EQ(commits.size(), 1u);
  if (commits.size() == 1) {
    CHECK_EQ(commits[0].subject, std::string("feat: first"));
    CHECK_EQ(commits[0].author_name, std::string("Ada Lovelace"));
    CHECK_EQ(commits[0].author_email, std::string("ada@example.com"));
    CHECK_EQ(commits[0].parents, 0);
    CHECK_EQ(commits[0].hash.size(), 40u);
    CHECK(!commits[0].files.empty());
    if (!commits[0].files.empty()) {
      CHECK_EQ(commits[0].files[0].path, std::string("README.md"));
      CHECK(commits[0].files[0].added >= 1);
    }
  }
}

}  // namespace

int main() {
  try {
    test_calendar();
    test_numbers_and_text();
    test_git_helpers();
    test_log_parser();
    test_log_command();
    test_analyze_helpers();
    test_analyzer();
    test_timeline_and_messages();
    test_roast();
    test_cli();
    test_render();
    test_terminal_layouts();
    test_languages();
    test_process_and_repo();
  } catch (const std::exception& error) {
    ++g_failed;
    std::fprintf(stderr, "FAIL uncaught exception: %s\n", error.what());
  }

  if (g_failed != 0) {
    std::fprintf(stderr, "%d failed, %d passed\n", g_failed, g_passed);
    return 1;
  }
  std::printf("%d passed\n", g_passed);
  return 0;
}
