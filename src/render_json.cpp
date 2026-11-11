#include "render.hpp"

#include <cstdio>
#include <vector>

#include "util.hpp"

namespace gitstory {

std::string json_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 2);
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20 || c == 0x7F) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        } else {
          out += ch;
        }
    }
  }
  return out;
}

namespace {

constexpr std::int64_t kDay = 86400;

// Minimal pretty-printing JSON writer; keeps track of commas and indentation.
class Json {
 public:
  void begin_object() { open('{'); }
  void end_object() { close('}'); }
  void begin_array() { open('['); }
  void end_array() { close(']'); }

  Json& key(std::string_view name) {
    separate();
    out_ += '"';
    out_ += json_escape(name);
    out_ += "\": ";
    after_key_ = true;
    return *this;
  }

  void value(std::string_view text) {
    separate();
    out_ += '"';
    out_ += json_escape(text);
    out_ += '"';
  }
  void value(const std::string& text) { value(std::string_view(text)); }
  void value(const char* text) { value(std::string_view(text)); }
  void value(std::int64_t number) {
    separate();
    out_ += std::to_string(number);
  }
  void value(int number) { value(static_cast<std::int64_t>(number)); }
  void value(bool flag) {
    separate();
    out_ += flag ? "true" : "false";
  }
  void value(double number) {
    separate();
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.4f", number);
    // snprintf follows the C locale's decimal separator; JSON requires '.'.
    for (char* p = buf; *p != '\0'; ++p) {
      if (*p == ',') *p = '.';
    }
    out_ += buf;
  }

  std::string take() {
    out_ += '\n';
    return std::move(out_);
  }

 private:
  void separate() {
    if (after_key_) {
      after_key_ = false;
      return;
    }
    if (!first_.empty()) {
      if (!first_.back()) out_ += ',';
      first_.back() = false;
      out_ += '\n';
      out_.append(first_.size() * 2, ' ');
    }
  }
  void open(char bracket) {
    separate();
    out_ += bracket;
    first_.push_back(true);
  }
  void close(char bracket) {
    const bool empty = first_.back();
    first_.pop_back();
    if (!empty) {
      out_ += '\n';
      out_.append(first_.size() * 2, ' ');
    }
    out_ += bracket;
  }

  std::string out_;
  std::vector<bool> first_;
  bool after_key_ = false;
};

void write_counts(Json& j, const std::vector<CountItem>& items, const char* label_key) {
  j.begin_array();
  for (const CountItem& item : items) {
    j.begin_object();
    j.key(label_key).value(item.label);
    j.key("count").value(item.count);
    j.end_object();
  }
  j.end_array();
}

void write_commit(Json& j, const CommitRef& c) {
  const std::int64_t local = c.time + c.tz_offset;
  j.begin_object();
  j.key("hash").value(c.hash);
  j.key("subject").value(c.subject);
  j.key("author").value(c.author);
  j.key("timestamp").value(c.time);
  j.key("local_date").value(format_iso_date(floor_div(local, kDay)));
  j.key("local_time").value(format_clock(static_cast<int>(floor_mod(local, kDay))));
  j.key("lines_added").value(c.added);
  j.key("lines_deleted").value(c.deleted);
  j.end_object();
}

}  // namespace

std::string render_json(const Stats& s, const RepoInfo& repo, Lang lang) {
  Json j;
  j.begin_object();
  j.key("gitstory_version").value(GITSTORY_VERSION);
  j.key("language").value(lang_code(lang));
  j.key("repository").value(repo.name);

  j.key("totals").begin_object();
  j.key("commits").value(s.commits);
  j.key("merges").value(s.merges);
  j.key("authors").value(s.author_count);
  j.key("lines_added").value(s.added);
  j.key("lines_deleted").value(s.deleted);
  j.key("files_touched").value(s.files_touched);
  j.key("binary_changes").value(s.binary_changes);
  j.end_object();

  if (s.commits > 0) {
    j.key("span").begin_object();
    j.key("first_commit").value(format_iso_date(s.first_day));
    j.key("last_commit").value(format_iso_date(s.last_day));
    j.key("first_timestamp").value(s.first_time);
    j.key("last_timestamp").value(s.last_time);
    j.key("days").value(s.span_days());
    j.key("active_days").value(s.active_days);
    j.end_object();

    j.key("rhythm").begin_object();
    j.key("by_hour").begin_array();
    for (const std::int64_t v : s.by_hour) j.value(v);
    j.end_array();
    j.key("by_weekday").begin_object();
    for (int wd = 0; wd < 7; ++wd) {
      j.key(weekday_name(wd)).value(s.by_weekday[static_cast<std::size_t>(wd)]);
    }
    j.end_object();
    j.key("night_commits").value(s.night_commits);
    j.key("weekend_commits").value(s.weekend_commits);
    j.key("friday_evening_commits").value(s.friday_evening_commits);
    j.key("night_examples").begin_array();
    for (const CommitRef& c : s.night_examples) write_commit(j, c);
    j.end_array();
    j.end_object();

    j.key("highlights").begin_object();
    j.key("busiest_day").begin_object();
    j.key("date").value(format_iso_date(s.busiest_day));
    j.key("commits").value(s.busiest_day_commits);
    j.end_object();
    j.key("busiest_week").begin_object();
    j.key("week_start").value(format_iso_date(s.busiest_week_start));
    j.key("commits").value(s.busiest_week_commits);
    j.end_object();
    j.key("busiest_month").begin_object();
    j.key("month").value(format_iso_date(s.busiest_month_day).substr(0, 7));
    j.key("commits").value(s.busiest_month_commits);
    j.end_object();
    j.key("longest_streak").begin_object();
    j.key("days").value(s.longest_streak);
    j.key("from").value(format_iso_date(s.streak_start));
    j.key("to").value(format_iso_date(s.streak_end));
    j.end_object();
    j.key("longest_break").begin_object();
    j.key("seconds").value(s.longest_gap_seconds);
    j.key("from_timestamp").value(s.gap_from);
    j.key("to_timestamp").value(s.gap_to);
    j.end_object();
    if (s.has_biggest_commit) {
      j.key("biggest_commit");
      write_commit(j, s.biggest_commit);
    }
    j.end_object();
  }

  j.key("authors").begin_array();
  for (const AuthorStat& a : s.authors) {
    j.begin_object();
    j.key("name").value(a.name);
    j.key("email").value(a.email);
    j.key("commits").value(a.commits);
    j.key("lines_added").value(a.added);
    j.key("lines_deleted").value(a.deleted);
    j.key("first_timestamp").value(a.first_time);
    j.key("last_timestamp").value(a.last_time);
    j.end_object();
  }
  j.end_array();

  j.key("top_files").begin_array();
  for (const FileStat& f : s.top_files) {
    j.begin_object();
    j.key("path").value(f.path);
    j.key("commits").value(f.commits);
    j.key("lines_added").value(f.added);
    j.key("lines_deleted").value(f.deleted);
    j.end_object();
  }
  j.end_array();

  j.key("file_types");
  write_counts(j, s.top_extensions, "extension");
  j.key("languages").begin_array();
  for (const LanguageStat& l : s.languages) {
    j.begin_object();
    j.key("name").value(l.name);
    j.key("lines_changed").value(l.lines);
    j.key("file_changes").value(l.changes);
    j.key("color").value(l.hex);
    j.end_object();
  }
  j.end_array();
  j.key("top_words");
  write_counts(j, s.top_words, "word");

  j.key("messages").begin_object();
  j.key("analysed").value(s.messages);
  j.key("average_length").value(s.avg_subject_length);
  j.key("low_effort").value(s.lazy_messages);
  j.key("low_effort_top");
  write_counts(j, s.lazy_top, "message");
  j.key("conventional_commits").value(s.conventional_commits);
  j.key("commit_types");
  write_counts(j, s.commit_types, "type");
  j.key("reverts").value(s.reverts);
  j.end_object();

  j.key("daily").begin_object();
  for (const auto& [day, count] : s.daily) j.key(format_iso_date(day)).value(count);
  j.end_object();

  j.key("roast").begin_array();
  for (const std::string& text : build_roast(s, lang)) j.value(text);
  j.end_array();

  j.end_object();
  return j.take();
}

}  // namespace gitstory
