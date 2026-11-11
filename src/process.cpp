#include "process.hpp"

#include <exception>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace gitstory {

std::string quote_windows_arg(std::string_view arg) {
  if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
    return std::string(arg);
  }
  std::string out = "\"";
  std::size_t backslashes = 0;
  for (const char c : arg) {
    if (c == '\\') {
      ++backslashes;
      continue;
    }
    if (c == '"') {
      // Escape every pending backslash, then the quote itself.
      out.append(backslashes * 2 + 1, '\\');
    } else {
      out.append(backslashes, '\\');
    }
    out += c;
    backslashes = 0;
  }
  // Backslashes before the closing quote must be doubled.
  out.append(backslashes * 2, '\\');
  out += '"';
  return out;
}

ProcessResult run_process(const std::vector<std::string>& argv, std::string& output) {
  return run_process(argv, [&output](std::string_view chunk) { output.append(chunk); });
}

#ifdef _WIN32

namespace {

std::wstring widen(std::string_view s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
  return out;
}

std::string last_error_text(DWORD code) {
  wchar_t* buffer = nullptr;
  const DWORD n = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::string out;
  if (n != 0 && buffer != nullptr) {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(n), nullptr, 0,
                                          nullptr, nullptr);
    out.resize(static_cast<std::size_t>(bytes));
    WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(n), out.data(), bytes, nullptr,
                        nullptr);
  }
  if (buffer != nullptr) LocalFree(buffer);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) {
    out.pop_back();
  }
  if (out.empty()) out = "error " + std::to_string(code);
  return out;
}

struct Handle {
  HANDLE h = nullptr;
  Handle() = default;
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  ~Handle() { close(); }
  void close() {
    if (h != nullptr && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    h = nullptr;
  }
  bool valid() const { return h != nullptr && h != INVALID_HANDLE_VALUE; }
};

void drain(HANDLE h, std::string& into) {
  char buf[4096];
  DWORD n = 0;
  while (ReadFile(h, buf, sizeof buf, &n, nullptr) && n > 0) into.append(buf, n);
}

}  // namespace

ProcessResult run_process(const std::vector<std::string>& argv, const ChunkSink& on_stdout) {
  ProcessResult result;
  if (argv.empty()) {
    result.launch_error = "empty command";
    return result;
  }

  std::string command_line;
  for (const std::string& arg : argv) {
    if (!command_line.empty()) command_line += ' ';
    command_line += quote_windows_arg(arg);
  }
  std::wstring wide_command = widen(command_line);

  SECURITY_ATTRIBUTES inheritable{};
  inheritable.nLength = sizeof inheritable;
  inheritable.bInheritHandle = TRUE;

  Handle out_read, out_write, err_read, err_write, nul;
  if (!CreatePipe(&out_read.h, &out_write.h, &inheritable, 1 << 16) ||
      !CreatePipe(&err_read.h, &err_write.h, &inheritable, 1 << 16)) {
    result.launch_error = last_error_text(GetLastError());
    return result;
  }
  // The parent's ends must not leak into the child, or the pipes never close.
  SetHandleInformation(out_read.h, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(err_read.h, HANDLE_FLAG_INHERIT, 0);
  nul.h = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable,
                      OPEN_EXISTING, 0, nullptr);

  STARTUPINFOW startup{};
  startup.cb = sizeof startup;
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = nul.valid() ? nul.h : nullptr;
  startup.hStdOutput = out_write.h;
  startup.hStdError = err_write.h;

  PROCESS_INFORMATION info{};
  if (!CreateProcessW(nullptr, wide_command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                      &startup, &info)) {
    result.launch_error = last_error_text(GetLastError());
    return result;
  }
  Handle process, thread;
  process.h = info.hProcess;
  thread.h = info.hThread;
  out_write.close();
  err_write.close();
  nul.close();

  result.started = true;
  std::thread err_reader([&] { drain(err_read.h, result.error_output); });

  std::exception_ptr failure;
  char buf[1 << 16];
  DWORD n = 0;
  while (ReadFile(out_read.h, buf, sizeof buf, &n, nullptr) && n > 0) {
    try {
      on_stdout(std::string_view(buf, n));
    } catch (...) {
      failure = std::current_exception();
      TerminateProcess(process.h, 1);
      break;
    }
  }
  out_read.close();
  err_reader.join();

  WaitForSingleObject(process.h, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(process.h, &code);
  result.exit_code = static_cast<int>(code);
  if (failure) std::rethrow_exception(failure);
  return result;
}

#else  // POSIX

namespace {

void drain(int fd, std::string& into) {
  char buf[4096];
  while (true) {
    const ssize_t n = read(fd, buf, sizeof buf);
    if (n > 0) {
      into.append(buf, static_cast<std::size_t>(n));
    } else if (n == 0 || errno != EINTR) {
      break;
    }
  }
}

}  // namespace

ProcessResult run_process(const std::vector<std::string>& argv, const ChunkSink& on_stdout) {
  ProcessResult result;
  if (argv.empty()) {
    result.launch_error = "empty command";
    return result;
  }

  int out_pipe[2], err_pipe[2], status_pipe[2];
  if (pipe(out_pipe) != 0) {
    result.launch_error = std::strerror(errno);
    return result;
  }
  if (pipe(err_pipe) != 0) {
    result.launch_error = std::strerror(errno);
    close(out_pipe[0]);
    close(out_pipe[1]);
    return result;
  }
  if (pipe(status_pipe) != 0) {
    result.launch_error = std::strerror(errno);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    return result;
  }
  // The status pipe closes by itself on a successful exec; if exec fails the
  // child writes errno into it, which is how the parent tells the two apart.
  fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);

  std::vector<char*> cargv;
  cargv.reserve(argv.size() + 1);
  for (const std::string& arg : argv) cargv.push_back(const_cast<char*>(arg.c_str()));
  cargv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid < 0) {
    result.launch_error = std::strerror(errno);
    for (int fd : {out_pipe[0], out_pipe[1], err_pipe[0], err_pipe[1], status_pipe[0],
                   status_pipe[1]}) {
      close(fd);
    }
    return result;
  }
  if (pid == 0) {
    const int devnull = open("/dev/null", O_RDONLY);
    if (devnull >= 0) dup2(devnull, STDIN_FILENO);
    dup2(out_pipe[1], STDOUT_FILENO);
    dup2(err_pipe[1], STDERR_FILENO);
    for (int fd : {out_pipe[0], out_pipe[1], err_pipe[0], err_pipe[1], status_pipe[0]}) close(fd);
    execvp(cargv[0], cargv.data());
    const int err = errno;
    const ssize_t ignored = write(status_pipe[1], &err, sizeof err);
    (void)ignored;
    _exit(127);
  }

  close(out_pipe[1]);
  close(err_pipe[1]);
  close(status_pipe[1]);

  int exec_errno = 0;
  ssize_t got = 0;
  do {
    got = read(status_pipe[0], &exec_errno, sizeof exec_errno);
  } while (got < 0 && errno == EINTR);
  close(status_pipe[0]);
  if (got == static_cast<ssize_t>(sizeof exec_errno)) {
    close(out_pipe[0]);
    close(err_pipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    result.launch_error = std::strerror(exec_errno);
    return result;
  }

  result.started = true;
  std::thread err_reader([&] { drain(err_pipe[0], result.error_output); });

  std::exception_ptr failure;
  char buf[1 << 16];
  while (true) {
    const ssize_t n = read(out_pipe[0], buf, sizeof buf);
    if (n > 0) {
      try {
        on_stdout(std::string_view(buf, static_cast<std::size_t>(n)));
      } catch (...) {
        failure = std::current_exception();
        kill(pid, SIGTERM);
        break;
      }
    } else if (n == 0 || errno != EINTR) {
      break;
    }
  }
  close(out_pipe[0]);
  err_reader.join();
  close(err_pipe[0]);

  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  }
  if (failure) std::rethrow_exception(failure);
  return result;
}

#endif

}  // namespace gitstory
