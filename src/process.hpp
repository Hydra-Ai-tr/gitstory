// Runs a child process without going through a shell, so arguments can never
// be reinterpreted (no cmd.exe %VAR% expansion, no quoting surprises).
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace gitstory {

struct ProcessResult {
  bool started = false;      // false when the executable could not be launched
  int exit_code = -1;
  std::string error_output;  // everything the child wrote to stderr
  std::string launch_error;  // OS error text when !started
};

using ChunkSink = std::function<void(std::string_view)>;

// Launches argv[0] (looked up on PATH) with the given arguments. stdout is
// streamed to `on_stdout` as it arrives; stderr is collected. stdin is closed.
// If the sink throws, the child is terminated and the exception rethrown.
ProcessResult run_process(const std::vector<std::string>& argv, const ChunkSink& on_stdout);

// Convenience wrapper that collects stdout into a string.
ProcessResult run_process(const std::vector<std::string>& argv, std::string& output);

// Quotes one argument following the MSVC runtime's command-line rules.
// Exposed so it can be unit-tested on every platform.
std::string quote_windows_arg(std::string_view arg);

}  // namespace gitstory
