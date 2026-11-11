// Small, dependency-free helpers: UTF-8 handling, number/date formatting and
// calendar arithmetic. Nothing in here touches the clock or the locale, so
// every function is deterministic and easy to test.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gitstory {

// ---- UTF-8 -----------------------------------------------------------------

// Decodes one code point starting at s[i] and advances i. On malformed input
// returns false, sets cp to U+FFFD and advances by exactly one byte.
bool decode_utf8(std::string_view s, std::size_t& i, char32_t& cp);
void append_utf8(std::string& out, char32_t cp);

// Makes arbitrary bytes safe to print: invalid UTF-8 becomes U+FFFD, control
// characters (including ESC) become spaces, bidi overrides are dropped, and
// surrounding whitespace is trimmed.
std::string sanitize_text(std::string_view s);

// Number of terminal columns the string occupies (wide CJK/emoji count as 2,
// combining marks as 0).
std::size_t display_width(std::string_view s);
std::size_t codepoint_count(std::string_view s);

// Shorten to at most `width` columns, keeping the start (or the end).
std::string truncate_right(std::string_view s, std::size_t width, std::string_view ellipsis);
std::string truncate_left(std::string_view s, std::size_t width, std::string_view ellipsis);
std::string pad_right(std::string_view s, std::size_t width);
std::string pad_left(std::string_view s, std::size_t width);

// Simple case folding covering ASCII, Latin-1, Latin Extended-A (incl.
// Turkish), Greek and Cyrillic.
char32_t to_lower(char32_t cp);
std::string lower_utf8(std::string_view s);

std::string trim(std::string_view s);
std::vector<std::string_view> split(std::string_view s, char sep, std::size_t max_fields = 0);

// ---- numbers ---------------------------------------------------------------

std::string format_number(std::int64_t n);  // 1234567 -> "1,234,567"
std::string format_percent(double ratio);   // 0.123   -> "12%"

// ---- calendar --------------------------------------------------------------

struct Civil {
  int year = 1970;
  unsigned month = 1;  // 1..12
  unsigned day = 1;    // 1..31
};

std::int64_t floor_div(std::int64_t a, std::int64_t b);
std::int64_t floor_mod(std::int64_t a, std::int64_t b);

// Days since 1970-01-01 in the proleptic Gregorian calendar.
std::int64_t days_from_civil(int year, unsigned month, unsigned day);
Civil civil_from_days(std::int64_t days);
int weekday_from_days(std::int64_t days);  // 0 = Monday .. 6 = Sunday

const char* weekday_name(int weekday);  // "Mon"
const char* month_name(unsigned month); // "Jan"

std::string format_date(std::int64_t day);      // "Mar 3, 2025"
std::string format_month(std::int64_t day);     // "Mar 2025"
std::string format_iso_date(std::int64_t day);  // "2025-03-03"
std::string format_clock(int seconds_of_day);   // "03:07"
std::string format_span_days(std::int64_t days);     // "5y 8m", "42 days"
std::string format_duration(std::int64_t seconds);   // "45 days", "7h", "12m"

}  // namespace gitstory
