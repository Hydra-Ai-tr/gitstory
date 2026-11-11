#include "platform.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>

#include <fcntl.h>
#include <io.h>
#include <shellapi.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace gitstory {

namespace {

std::size_t columns_from_env() {
  const char* columns = std::getenv("COLUMNS");
  if (columns == nullptr) return 0;
  const long value = std::strtol(columns, nullptr, 10);
  return value > 0 && value < 10000 ? static_cast<std::size_t>(value) : 0;
}

#ifdef _WIN32

std::string narrow(const wchar_t* text, int length) {
  if (length <= 0) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<std::size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, length, out.data(), n, nullptr, nullptr);
  return out;
}

// Git Bash / MSYS2 / Cygwin terminals hand native programs a named pipe rather
// than a console, so _isatty() says "no". The pipe's name gives them away.
bool is_msys_pty(HANDLE handle) {
  if (GetFileType(handle) != FILE_TYPE_PIPE) return false;
  alignas(FILE_NAME_INFO) char buffer[sizeof(FILE_NAME_INFO) + MAX_PATH * sizeof(wchar_t)];
  if (!GetFileInformationByHandleEx(handle, FileNameInfo, buffer, sizeof buffer)) return false;
  const auto* info = reinterpret_cast<const FILE_NAME_INFO*>(buffer);
  const std::wstring name(info->FileName, info->FileNameLength / sizeof(wchar_t));
  const bool family =
      name.find(L"msys-") != std::wstring::npos || name.find(L"cygwin-") != std::wstring::npos;
  return family && name.find(L"-pty") != std::wstring::npos;
}

#endif

}  // namespace

Lang preferred_language() {
  for (const char* name : {"GITSTORY_LANG", "LC_ALL", "LC_MESSAGES", "LANG"}) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') continue;
    // "C" and "POSIX" (also "C.UTF-8") say nothing about the user's language.
    if (std::strcmp(value, "POSIX") == 0 || (value[0] == 'C' && (value[1] == '\0' || value[1] == '.'))) {
      continue;
    }
    return parse_lang(value).value_or(Lang::En);
  }
#ifdef _WIN32
  switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_TURKISH: return Lang::Tr;
    case LANG_RUSSIAN: return Lang::Ru;
    default: break;
  }
#endif
  return Lang::En;
}

#ifdef _WIN32

std::vector<std::string> utf8_args(int, char**) {
  std::vector<std::string> args;
  int count = 0;
  wchar_t** wide = CommandLineToArgvW(GetCommandLineW(), &count);
  if (wide == nullptr) return args;
  for (int i = 1; i < count; ++i) {
    args.push_back(narrow(wide[i], static_cast<int>(std::wcslen(wide[i]))));
  }
  LocalFree(wide);
  return args;
}

ConsoleSession::ConsoleSession() {
  // Bytes go out untouched: no "\n" -> "\r\n" translation in redirected output.
  _setmode(_fileno(stdout), _O_BINARY);

  const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (handle != INVALID_HANDLE_VALUE && handle != nullptr && GetConsoleMode(handle, &mode)) {
    is_terminal_ = true;
    saved_codepage_ = GetConsoleOutputCP();
    if (saved_codepage_ != CP_UTF8 && SetConsoleOutputCP(CP_UTF8)) restore_codepage_ = true;
    saved_mode_ = mode;
    // Windows Terminal, VS Code/Cursor, ConEmu, WezTerm and friends announce
    // themselves; a console without any of these is the legacy conhost.
    full_glyphs_ = false;
    for (const char* name : {"WT_SESSION", "TERM_PROGRAM", "ConEmuANSI", "WEZTERM_PANE",
                             "ALACRITTY_WINDOW_ID", "TERM"}) {
      const char* value = std::getenv(name);
      if (value != nullptr && value[0] != '\0' && std::strcmp(value, "dumb") != 0) {
        full_glyphs_ = true;
      }
    }
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
      supports_color_ = true;
    } else if (SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
      supports_color_ = true;
      restore_mode_ = true;
    }
  } else if (handle != INVALID_HANDLE_VALUE && handle != nullptr && is_msys_pty(handle)) {
    is_terminal_ = true;
    supports_color_ = true;
  }
}

ConsoleSession::~ConsoleSession() {
  std::fflush(stdout);
  if (restore_mode_) SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), saved_mode_);
  if (restore_codepage_) SetConsoleOutputCP(saved_codepage_);
}

std::size_t terminal_width() {
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
    const int width = info.srWindow.Right - info.srWindow.Left + 1;
    if (width > 0) return static_cast<std::size_t>(width);
  }
  return columns_from_env();
}

#else  // POSIX

std::vector<std::string> utf8_args(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  return args;
}

ConsoleSession::ConsoleSession() {
  is_terminal_ = isatty(STDOUT_FILENO) != 0;
  const char* term = std::getenv("TERM");
  supports_color_ = is_terminal_ && term != nullptr && std::strcmp(term, "dumb") != 0;
}

ConsoleSession::~ConsoleSession() { std::fflush(stdout); }

std::size_t terminal_width() {
  struct winsize size {};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) return size.ws_col;
  return columns_from_env();
}

#endif

void write_stdout(std::string_view data) {
  std::fwrite(data.data(), 1, data.size(), stdout);
  std::fflush(stdout);
}

bool write_file(const std::string& path, std::string_view data, std::string& error) {
  // u8path keeps non-ASCII file names intact on Windows.
  std::ofstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
  if (!file) {
    error = std::strerror(errno);
    return false;
  }
  file.write(data.data(), static_cast<std::streamsize>(data.size()));
  file.close();
  if (!file) {
    error = std::strerror(errno);
    return false;
  }
  return true;
}

}  // namespace gitstory
