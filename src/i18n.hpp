// Report languages: every user-facing word, number and date format of the
// terminal report, the SVG card and the roast lives behind a Locale.
// JSON keys and ISO dates stay language-neutral.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gitstory {

enum class Lang { En, Tr, Ru };

// Accepts codes ("tr"), locale strings ("tr_TR.UTF-8", "ru-RU") and names
// ("turkish", "türkçe", "русский").
std::optional<Lang> parse_lang(std::string_view text);
const char* lang_code(Lang lang);

// A noun with its plural forms. English uses `one` and `many`; Turkish keeps
// the singular after numbers, so all three match; Russian needs all three
// (1 коммит, 2 коммита, 5 коммитов).
struct Noun {
  const char* one;
  const char* few;
  const char* many;
};

// Templates use {0}..{9} placeholders; the comment on each field lists them.
struct Strings {
  Noun commit, author, day, line, file, merge, character, time, revert, message, person, month,
      year;

  const char* no_commits;
  const char* unknown_author;
  const char* no_message;

  const char* section_activity;
  const char* section_rhythm;
  const char* section_highlights;
  const char* section_authors;
  const char* section_languages;
  const char* section_files;
  const char* section_words;
  const char* section_messages;
  const char* section_roast;

  const char* story_one_day;  // {0} repo, {1} date, {2} commits, {3} who
  const char* story_span;     // {0} repo, {1} span, {2} from, {3} to, {4} commits, {5} who
  const char* who_one;        // {0} name
  const char* who_two;        // {0} name, {1} name
  const char* who_many;       // {0} "132 people", {1} "132"
  const char* story_active;   // {0} active days, {1} days in span
  const char* story_rhythm;   // {0} weekday, {1} hour
  const char* story_night;    // {0} percent  (continues the rhythm sentence)
  const char* story_weekend;  // {0} percent
  const char* story_grew;     // {0} "14,484 lines"
  const char* story_shrank;   // {0} "120 lines"
  const char* story_top_file; // {0} path     (continues the growth sentence)
  const char* story_lang_one; // {0} language
  const char* story_lang_two; // {0} language, {1} language
  const char* story_lead;     // {0} name, {1} percent

  const char* tile_active_days;
  const char* tile_lines;

  const char* activity_one_day;  // {0} date
  const char* activity_summary;  // {0} "41 commits", {1} active days, {2} days in span
  const char* busiest;
  const char* less;
  const char* more;
  const char* each_column;  // {0} "36 days"

  const char* phase_night;
  const char* phase_morning;
  const char* phase_afternoon;
  const char* phase_evening;
  const char* busiest_day;
  const char* busiest_hour;
  const char* weekends;
  const char* friday_late;

  const char* hl_busiest_day;
  const char* hl_busiest_week;
  const char* hl_busiest_month;
  const char* hl_streak;
  const char* hl_break;
  const char* hl_biggest;
  const char* hl_night;
  const char* week_of;        // {0} date
  const char* night_between;  // {0} "225 commits", {1} percent

  const char* more_authors;  // {0} number

  const char* msg_average;
  const char* msg_conventional;
  const char* msg_lazy;
  const char* msg_reverts;
  const char* verdict_terse;
  const char* verdict_to_the_point;
  const char* verdict_detailed;
  const char* of_commits;  // {0} percent

  const char* roast_night;         // {0} percent
  const char* roast_healthy;
  const char* roast_lazy;          // {0} "12 commit messages", {1} roast_lazy_top or ""
  const char* roast_lazy_top;      // {0} message, {1} "5 times"
  const char* roast_short;         // {0} number, {1} "12 characters"
  const char* roast_silent;        // {0} duration, {1} date
  const char* roast_big;           // {0} "39,645 lines"
  const char* roast_weekend;       // {0} percent
  const char* roast_friday;        // {0} "11 commits"
  const char* roast_hot_file;      // {0} path, {1} percent
  const char* roast_solo;          // {0} "41 commits"
  const char* roast_dominant;      // {0} name, {1} percent
  const char* roast_reverts;       // {0} "3 reverts"
  const char* roast_streak;        // {0} number, {1} "14 days"
  const char* roast_conventional;
  const char* roast_nothing;

  const char* svg_when;
  const char* svg_night;
  const char* svg_weekend;
  const char* svg_peak_hour;
  const char* svg_activity;
  const char* svg_words;
};

class Locale {
 public:
  explicit Locale(Lang lang = Lang::En);

  Lang lang() const { return lang_; }
  const Strings& text() const { return *text_; }

  std::string number(std::int64_t n) const;  // 1,234 / 1.234 / 1 234
  std::string percent(double ratio) const;   // 19% / %19 / 19%
  std::string percent1(double ratio) const;  // 70.9% / %70,9 / 70,9%
  std::string compact(std::int64_t n) const; // 12.3k / 12,3k
  const char* noun(std::int64_t n, const Noun& noun) const;
  std::string count(std::int64_t n, const Noun& noun) const;  // "41 commits"

  const char* weekday_short(int weekday) const;        // Mon / Pzt / Пн
  const char* weekday(int weekday) const;              // Monday / Pazartesi / Понедельник
  const char* weekday_in_sentence(int weekday) const;  // Monday / pazartesi / понедельник
  const char* month_short(unsigned month) const;       // Jan / Oca / янв

  std::string date(std::int64_t day) const;                       // Mar 3, 2025 / 3 Mar 2025
  std::string day_month(std::int64_t day, bool with_year) const;  // Mar 3 / 3 Mar
  std::string month_year(std::int64_t day) const;                 // Mar 2025
  std::string span(std::int64_t days) const;        // 8y 8m / 8 yıl 8 ay
  std::string span_prose(std::int64_t days) const;  // 8 years and 8 months
  std::string duration(std::int64_t seconds) const; // 45 days / 7h / 12m

  // "Other", "Lockfiles", "Images" and "Text" are categories, not languages.
  std::string language_name(const std::string& name) const;

 private:
  Lang lang_;
  const Strings* text_;
};

// Replaces {0}..{9} with `args`.
std::string fill(std::string_view tmpl, const std::vector<std::string>& args);

struct TemplatePart {
  std::string_view text;  // literal text, when arg < 0
  int arg = -1;
};
std::vector<TemplatePart> split_template(std::string_view tmpl);

}  // namespace gitstory
