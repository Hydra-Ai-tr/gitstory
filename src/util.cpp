#include "util.hpp"

#include <cstdio>

namespace gitstory {

// ---- UTF-8 -----------------------------------------------------------------

bool decode_utf8(std::string_view s, std::size_t& i, char32_t& cp) {
  const auto c = static_cast<unsigned char>(s[i]);
  if (c < 0x80) {
    cp = c;
    i += 1;
    return true;
  }
  std::size_t len = 0;
  char32_t min = 0;
  if ((c & 0xE0) == 0xC0) {
    len = 2;
    cp = c & 0x1F;
    min = 0x80;
  } else if ((c & 0xF0) == 0xE0) {
    len = 3;
    cp = c & 0x0F;
    min = 0x800;
  } else if ((c & 0xF8) == 0xF0) {
    len = 4;
    cp = c & 0x07;
    min = 0x10000;
  }
  bool ok = len != 0 && i + len <= s.size();
  for (std::size_t k = 1; ok && k < len; ++k) {
    const auto cc = static_cast<unsigned char>(s[i + k]);
    if ((cc & 0xC0) != 0x80) {
      ok = false;
    } else {
      cp = (cp << 6) | (cc & 0x3F);
    }
  }
  if (ok && (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))) ok = false;
  if (!ok) {
    cp = 0xFFFD;
    i += 1;
    return false;
  }
  i += len;
  return true;
}

void append_utf8(std::string& out, char32_t cp) {
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

namespace {

bool is_bidi_control(char32_t cp) {
  return (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2066 && cp <= 0x2069);
}

bool is_control(char32_t cp) { return cp < 0x20 || (cp >= 0x7F && cp <= 0x9F); }

int char_width(char32_t cp) {
  if (cp == 0) return 0;
  // Zero width: combining marks, joiners, variation selectors.
  if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
      (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x200B && cp <= 0x200F) ||
      (cp >= 0x20D0 && cp <= 0x20FF) || (cp >= 0xFE00 && cp <= 0xFE0F) ||
      (cp >= 0xFE20 && cp <= 0xFE2F) || (cp >= 0xE0100 && cp <= 0xE01EF) || cp == 0x2060 ||
      cp == 0xFEFF) {
    return 0;
  }
  // Double width: CJK, Hangul, fullwidth forms, emoji.
  if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0x303E) ||
      (cp >= 0x3041 && cp <= 0x33FF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
      (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xA000 && cp <= 0xA4CF) ||
      (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
      (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
      (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1F64F) ||
      (cp >= 0x1F680 && cp <= 0x1F6FF) || (cp >= 0x1F900 && cp <= 0x1FAFF) ||
      (cp >= 0x20000 && cp <= 0x3FFFD)) {
    return 2;
  }
  return 1;
}

struct Glyph {
  std::size_t offset;
  std::size_t length;
  int width;
};

std::vector<Glyph> glyphs_of(std::string_view s) {
  std::vector<Glyph> out;
  std::size_t i = 0;
  while (i < s.size()) {
    const std::size_t start = i;
    char32_t cp = 0;
    decode_utf8(s, i, cp);
    out.push_back({start, i - start, char_width(cp)});
  }
  return out;
}

}  // namespace

std::string sanitize_text(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  std::size_t i = 0;
  while (i < s.size()) {
    char32_t cp = 0;
    decode_utf8(s, i, cp);
    if (is_bidi_control(cp)) continue;
    if (is_control(cp)) cp = ' ';
    append_utf8(out, cp);
  }
  return trim(out);
}

std::size_t display_width(std::string_view s) {
  std::size_t width = 0;
  std::size_t i = 0;
  while (i < s.size()) {
    char32_t cp = 0;
    decode_utf8(s, i, cp);
    width += static_cast<std::size_t>(char_width(cp));
  }
  return width;
}

std::size_t codepoint_count(std::string_view s) {
  std::size_t count = 0;
  std::size_t i = 0;
  while (i < s.size()) {
    char32_t cp = 0;
    decode_utf8(s, i, cp);
    ++count;
  }
  return count;
}

std::string truncate_right(std::string_view s, std::size_t width, std::string_view ellipsis) {
  if (display_width(s) <= width) return std::string(s);
  const std::size_t ell = display_width(ellipsis);
  if (width <= ell) return width == ell ? std::string(ellipsis) : std::string();
  const std::size_t budget = width - ell;
  std::size_t used = 0;
  std::size_t end = 0;
  for (const Glyph& g : glyphs_of(s)) {
    if (used + static_cast<std::size_t>(g.width) > budget) break;
    used += static_cast<std::size_t>(g.width);
    end = g.offset + g.length;
  }
  std::string out(s.substr(0, end));
  out += ellipsis;
  return out;
}

std::string truncate_left(std::string_view s, std::size_t width, std::string_view ellipsis) {
  if (display_width(s) <= width) return std::string(s);
  const std::size_t ell = display_width(ellipsis);
  if (width <= ell) return width == ell ? std::string(ellipsis) : std::string();
  const std::size_t budget = width - ell;
  const std::vector<Glyph> glyphs = glyphs_of(s);
  std::size_t used = 0;
  std::size_t begin = s.size();
  for (auto it = glyphs.rbegin(); it != glyphs.rend(); ++it) {
    if (used + static_cast<std::size_t>(it->width) > budget) break;
    used += static_cast<std::size_t>(it->width);
    begin = it->offset;
  }
  std::string out(ellipsis);
  out += s.substr(begin);
  return out;
}

std::string pad_right(std::string_view s, std::size_t width) {
  std::string out(s);
  const std::size_t w = display_width(s);
  if (w < width) out.append(width - w, ' ');
  return out;
}

std::string pad_left(std::string_view s, std::size_t width) {
  const std::size_t w = display_width(s);
  std::string out;
  if (w < width) out.append(width - w, ' ');
  out += s;
  return out;
}

char32_t to_lower(char32_t cp) {
  if (cp >= 'A' && cp <= 'Z') return cp + 0x20;
  if (cp < 0xC0) return cp;
  if (cp <= 0xDE) return cp == 0xD7 ? cp : cp + 0x20;  // Latin-1
  if (cp < 0x100) return cp;
  if (cp <= 0x17F) {  // Latin Extended-A
    if (cp == 0x130) return 'i';  // İ
    if (cp == 0x131 || cp == 0x138 || cp == 0x149 || cp == 0x17F) return cp;
    if (cp == 0x178) return 0xFF;
    const bool upper_is_even = (cp <= 0x137) || (cp >= 0x14A && cp <= 0x177);
    if (upper_is_even) return (cp % 2 == 0) ? cp + 1 : cp;
    return (cp % 2 == 1) ? cp + 1 : cp;
  }
  if (cp >= 0x391 && cp <= 0x3A9 && cp != 0x3A2) return cp + 0x20;  // Greek
  if (cp >= 0x400 && cp <= 0x40F) return cp + 0x50;                  // Cyrillic
  if (cp >= 0x410 && cp <= 0x42F) return cp + 0x20;
  return cp;
}

std::string lower_utf8(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  std::size_t i = 0;
  while (i < s.size()) {
    char32_t cp = 0;
    decode_utf8(s, i, cp);
    append_utf8(out, to_lower(cp));
  }
  return out;
}

std::string trim(std::string_view s) {
  const char* ws = " \t\r\n\v\f";
  const std::size_t a = s.find_first_not_of(ws);
  if (a == std::string_view::npos) return {};
  const std::size_t b = s.find_last_not_of(ws);
  return std::string(s.substr(a, b - a + 1));
}

std::vector<std::string_view> split(std::string_view s, char sep, std::size_t max_fields) {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  while (true) {
    if (max_fields != 0 && out.size() + 1 == max_fields) {
      out.push_back(s.substr(start));
      break;
    }
    const std::size_t pos = s.find(sep, start);
    if (pos == std::string_view::npos) {
      out.push_back(s.substr(start));
      break;
    }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
  return out;
}

// ---- numbers ---------------------------------------------------------------

std::string format_number(std::int64_t n) {
  const bool negative = n < 0;
  // Work on the magnitude as unsigned so INT64_MIN is handled too.
  std::uint64_t mag = negative ? 0 - static_cast<std::uint64_t>(n) : static_cast<std::uint64_t>(n);
  std::string digits = std::to_string(mag);
  std::string out;
  const std::size_t lead = digits.size() % 3;
  for (std::size_t i = 0; i < digits.size(); ++i) {
    if (i != 0 && (i % 3) == lead) out += ',';
    out += digits[i];
  }
  return negative ? "-" + out : out;
}

std::string format_percent(double ratio) {
  const double pct = ratio * 100.0;
  char buf[32];
  if (pct > 0.0 && pct < 1.0) {
    std::snprintf(buf, sizeof buf, "<1%%");
  } else {
    std::snprintf(buf, sizeof buf, "%.0f%%", pct);
  }
  return buf;
}

// ---- calendar --------------------------------------------------------------

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
  std::int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}

std::int64_t floor_mod(std::int64_t a, std::int64_t b) { return a - floor_div(a, b) * b; }

// Howard Hinnant's civil calendar algorithms.
std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
  std::int64_t y = year;
  y -= month <= 2;
  const std::int64_t era = floor_div(y, 400);
  const auto yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

Civil civil_from_days(std::int64_t days) {
  days += 719468;
  const std::int64_t era = floor_div(days, 146097);
  const auto doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  Civil c;
  c.day = doy - (153 * mp + 2) / 5 + 1;
  c.month = mp < 10 ? mp + 3 : mp - 9;
  c.year = static_cast<int>(y + (c.month <= 2));
  return c;
}

int weekday_from_days(std::int64_t days) {
  // 1970-01-01 was a Thursday (index 3 when Monday is 0).
  return static_cast<int>(floor_mod(days + 3, 7));
}

const char* weekday_name(int weekday) {
  static const char* names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  return (weekday >= 0 && weekday < 7) ? names[weekday] : "???";
}

const char* month_name(unsigned month) {
  static const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return (month >= 1 && month <= 12) ? names[month - 1] : "???";
}

std::string format_date(std::int64_t day) {
  const Civil c = civil_from_days(day);
  return std::string(month_name(c.month)) + " " + std::to_string(c.day) + ", " +
         std::to_string(c.year);
}

std::string format_month(std::int64_t day) {
  const Civil c = civil_from_days(day);
  return std::string(month_name(c.month)) + " " + std::to_string(c.year);
}

std::string format_iso_date(std::int64_t day) {
  const Civil c = civil_from_days(day);
  char buf[48];
  std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", c.year, c.month, c.day);
  return buf;
}

std::string format_clock(int seconds_of_day) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%02d:%02d", (seconds_of_day / 3600) % 24,
                (seconds_of_day / 60) % 60);
  return buf;
}

std::string format_span_days(std::int64_t days) {
  if (days < 0) days = 0;
  if (days == 1) return "1 day";
  if (days < 60) return std::to_string(days) + " days";
  const auto months = static_cast<std::int64_t>(static_cast<double>(days) / 30.4375);
  const std::int64_t years = months / 12;
  const std::int64_t rest = months % 12;
  if (years == 0) return std::to_string(months) + " months";
  if (rest == 0) return std::to_string(years) + (years == 1 ? " year" : " years");
  return std::to_string(years) + "y " + std::to_string(rest) + "m";
}

std::string format_duration(std::int64_t seconds) {
  if (seconds < 0) seconds = 0;
  if (seconds >= 2 * 86400) return format_number(seconds / 86400) + " days";
  if (seconds >= 3600) return std::to_string(seconds / 3600) + "h";
  return std::to_string(seconds / 60) + "m";
}

}  // namespace gitstory
