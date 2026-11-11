// The few things that genuinely differ between Windows and POSIX terminals.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "i18n.hpp"

namespace gitstory {

// The report language when --lang is not given: GITSTORY_LANG, then the
// LC_ALL / LC_MESSAGES / LANG locale, then (on Windows) the display language.
// Anything other than Turkish or Russian falls back to English.
Lang preferred_language();

// Command-line arguments as UTF-8, without the program name. On Windows the
// narrow argv is in the ANSI code page and would mangle non-ASCII paths.
std::vector<std::string> utf8_args(int argc, char** argv);

// Puts the console into UTF-8 / ANSI-escape mode for the lifetime of the
// object and restores the previous state afterwards. A no-op on POSIX.
class ConsoleSession {
 public:
  ConsoleSession();
  ~ConsoleSession();
  ConsoleSession(const ConsoleSession&) = delete;
  ConsoleSession& operator=(const ConsoleSession&) = delete;

  bool is_terminal() const { return is_terminal_; }
  bool supports_color() const { return supports_color_; }
  // False for the classic Windows console host, whose fonts lack the partial
  // block characters modern terminals draw charts with.
  bool full_glyphs() const { return full_glyphs_; }

 private:
  bool is_terminal_ = false;
  bool supports_color_ = false;
  bool full_glyphs_ = true;
#ifdef _WIN32
  unsigned saved_codepage_ = 0;
  unsigned long saved_mode_ = 0;
  bool restore_codepage_ = false;
  bool restore_mode_ = false;
#endif
};

// Width of the terminal attached to stdout, or 0 when unknown.
std::size_t terminal_width();

// Writes bytes to stdout exactly as given (no newline translation).
void write_stdout(std::string_view data);

// Writes a file whose name is UTF-8. Returns false and fills `error` on failure.
bool write_file(const std::string& path, std::string_view data, std::string& error);

}  // namespace gitstory
