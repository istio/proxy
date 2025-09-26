// Copyright 2019 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "process.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>
#else
extern "C" {
  extern char **environ;
}
#endif

std::map<std::string, std::string> GetCurrentEnvironment() {
  std::map<std::string, std::string> result;
  char **envp = environ;
  for (int i = 0; envp[i] != nullptr; ++i) {
    std::string envString(envp[i]);
    size_t equalsPos = envString.find('=');
    if (equalsPos != std::string::npos) {
        std::string key = envString.substr(0, equalsPos);
        std::string value = envString.substr(equalsPos + 1);
        result[key] = value;
    }
  }
  return result;
}

#if defined(_WIN32)

namespace {
class WindowsIORedirector {
  enum { In, Out };
  enum { Rd, Wr };

  HANDLE hIO_[2][2];

  explicit WindowsIORedirector(HANDLE hIO[2][2], bool include_stdout) {
    std::memcpy(hIO_, hIO, sizeof(hIO_));
    assert(hIO_[In][Rd] == INVALID_HANDLE_VALUE);
    assert(hIO_[In][Wr] == INVALID_HANDLE_VALUE);

    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(siStartInfo);
    siStartInfo.dwFlags = STARTF_USESTDHANDLES;
    siStartInfo.hStdInput = INVALID_HANDLE_VALUE;
    siStartInfo.hStdOutput =
        include_stdout ? hIO_[Out][Wr] : INVALID_HANDLE_VALUE;
    siStartInfo.hStdError = hIO_[Out][Wr];
  }

 public:
  STARTUPINFOA siStartInfo;

  WindowsIORedirector(const WindowsIORedirector &) = delete;
  WindowsIORedirector &operator=(const WindowsIORedirector &) = delete;

  WindowsIORedirector(WindowsIORedirector &&) = default;
  WindowsIORedirector &operator=(WindowsIORedirector &&) = default;

  ~WindowsIORedirector() {
    assert(hIO_[In][Rd] == INVALID_HANDLE_VALUE);
    assert(hIO_[In][Wr] == INVALID_HANDLE_VALUE);
    CloseHandle(hIO_[Out][Rd]);
    CloseHandle(hIO_[Out][Wr]);
  }

  static std::unique_ptr<WindowsIORedirector> Create(bool include_stdout,
                                                     std::error_code &ec) {
    HANDLE hIO[2][2] = {
      {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE},
      {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE},
    };

    SECURITY_ATTRIBUTES saAttr;
    ZeroMemory(&saAttr, sizeof(saAttr));
    saAttr.nLength = sizeof(saAttr);
    saAttr.lpSecurityDescriptor = nullptr;
    saAttr.bInheritHandle = TRUE;

    if (!CreatePipe(&hIO[Out][Rd], &hIO[Out][Wr], &saAttr, 0)) {
      ec = std::error_code(GetLastError(), std::system_category());
      return nullptr;
    }

    // The read handle for stdout should not be inheritted by the child.
    if (!SetHandleInformation(hIO[Out][Rd], HANDLE_FLAG_INHERIT, FALSE)) {
      ec = std::error_code(GetLastError(), std::system_category());
      CloseHandle(hIO[Out][Rd]);
      CloseHandle(hIO[Out][Wr]);
      return nullptr;
    }

    return std::unique_ptr<WindowsIORedirector>(
        new WindowsIORedirector(hIO, include_stdout));
  }

  void ConsumeAllSubprocessOutput(std::ostream *stderr_stream);
};

void WindowsIORedirector::ConsumeAllSubprocessOutput(
    std::ostream *stderr_stream) {
  CloseHandle(hIO_[Out][Wr]);
  hIO_[Out][Wr] = INVALID_HANDLE_VALUE;

  char stderr_buffer[1024];
  DWORD dwNumberOfBytesRead;
  while (ReadFile(hIO_[Out][Rd], stderr_buffer, sizeof(stderr_buffer),
                  &dwNumberOfBytesRead, nullptr)) {
    if (dwNumberOfBytesRead)
      stderr_stream->write(stderr_buffer, dwNumberOfBytesRead);
  }
  if (dwNumberOfBytesRead)
    stderr_stream->write(stderr_buffer, dwNumberOfBytesRead);
}

std::string GetCommandLine(const std::vector<std::string> &arguments) {
  // To escape the command line, we surround the argument with quotes.
  // However, the complication comes due to how the Windows command line
  // parser treats backslashes (\) and quotes (").
  //
  // - \ is normally treated as a literal backslash
  //      e.g. alpha\beta\gamma => alpha\beta\gamma
  // - The sequence \" is treated as a literal "
  //      e.g. alpha\"beta => alpha"beta
  //
  // But then what if we are given a path that ends with a \?
  //
  // Surrounding alpha\beta\ with " would be "alpha\beta\" which would be
  // an unterminated string since it ends on a literal quote. To allow
  // this case the parser treats:
  //
  //  - \\" as \ followed by the " metacharacter
  //  - \\\" as \ followed by a literal "
  //
  // In general:
  //  - 2n \ followed by " => n \ followed by the " metacharacter
  //  - 2n + 1 \ followed by " => n \ followed by a literal "
  auto quote = [](const std::string &argument) -> std::string {
    if (argument.find_first_of(" \t\n\"") == std::string::npos) return argument;

    std::ostringstream buffer;

    buffer << '\"';
    std::string::const_iterator cur = std::begin(argument);
    std::string::const_iterator end = std::end(argument);
    while (cur < end) {
      std::string::size_type offset = std::distance(std::begin(argument), cur);

      std::string::size_type start = argument.find_first_not_of('\\', offset);
      if (start == std::string::npos) {
        // String ends with a backslash (e.g. first\second\), escape all
        // the backslashes then add the metacharacter ".
        buffer << std::string(2 * (argument.length() - offset), '\\');
        break;
      }

      std::string::size_type count = start - offset;
      // If this is a string of \ followed by a " (e.g. first\"second).
      // Escape the backslashes and the quote, otherwise these are just literal
      // backslashes.
      buffer << std::string(argument.at(start) == '\"' ? 2 * count + 1 : count,
                            '\\')
             << argument.at(start);
      // Drop the backslashes and the following character.
      std::advance(cur, count + 1);
    }
    buffer << '\"';

    return buffer.str();
  };

  std::ostringstream quoted;
  std::transform(std::begin(arguments), std::end(arguments),
                 std::ostream_iterator<std::string>(quoted, " "), quote);
  return quoted.str();
}
}  // namespace

int RunSubProcess(const std::vector<std::string> &args,
                  std::map<std::string, std::string> *env,
                  std::ostream *stderr_stream, bool stdout_to_stderr) {
  std::error_code ec;
  std::unique_ptr<WindowsIORedirector> redirector =
      WindowsIORedirector::Create(stdout_to_stderr, ec);
  if (!redirector) {
    (*stderr_stream) << "unable to create stderr pipe: " << ec.message()
                     << '\n';
    return 254;
  }

  PROCESS_INFORMATION piProcess = {0};
  if (!CreateProcessA(NULL, GetCommandLine(args).data(), nullptr, nullptr, TRUE,
                      0, nullptr, nullptr, &redirector->siStartInfo,
                      &piProcess)) {
    DWORD dwLastError = GetLastError();
    (*stderr_stream) << "unable to create process (error " << dwLastError
                     << ")\n";
    return dwLastError;
  }

  CloseHandle(piProcess.hThread);

  redirector->ConsumeAllSubprocessOutput(stderr_stream);

  if (WaitForSingleObject(piProcess.hProcess, INFINITE) == WAIT_FAILED) {
    DWORD dwLastError = GetLastError();
    (*stderr_stream) << "wait for process failure (error " << dwLastError
                     << ")\n";
    CloseHandle(piProcess.hProcess);
    return dwLastError;
  }

  DWORD dwExitCode;
  if (!GetExitCodeProcess(piProcess.hProcess, &dwExitCode)) {
    DWORD dwLastError = GetLastError();
    (*stderr_stream) << "unable to get exit code (error " << dwLastError
                     << ")\n";
    CloseHandle(piProcess.hProcess);
    return dwLastError;
  }

  CloseHandle(piProcess.hProcess);
  return dwExitCode;
}

#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <memory>

namespace {

// An RAII class that manages the pipes and posix_spawn state needed to redirect
// subprocess I/O. Currently only supports stderr, but can be extended to handle
// stdin and stdout if needed.
class PosixSpawnIORedirector {
 public:
  // Create an I/O redirector that can be used with posix_spawn to capture
  // stderr.
  static std::unique_ptr<PosixSpawnIORedirector> Create(bool stdoutToStderr) {
    int stderr_pipe[2];
    if (pipe(stderr_pipe) != 0) {
      return nullptr;
    }

    return std::unique_ptr<PosixSpawnIORedirector>(
        new PosixSpawnIORedirector(stderr_pipe, stdoutToStderr));
  }

  // Explicitly make PosixSpawnIORedirector non-copyable and movable.
  PosixSpawnIORedirector(const PosixSpawnIORedirector &) = delete;
  PosixSpawnIORedirector &operator=(const PosixSpawnIORedirector &) = delete;
  PosixSpawnIORedirector(PosixSpawnIORedirector &&) = default;
  PosixSpawnIORedirector &operator=(PosixSpawnIORedirector &&) = default;

  ~PosixSpawnIORedirector() {
    SafeClose(&stderr_pipe_[0]);
    SafeClose(&stderr_pipe_[1]);
    posix_spawn_file_actions_destroy(&file_actions_);
  }

  // Returns the pointer to a posix_spawn_file_actions_t value that should be
  // passed to posix_spawn to enable this redirection.
  posix_spawn_file_actions_t *PosixSpawnFileActions() { return &file_actions_; }

  // Returns a pointer to the two-element file descriptor array for the stderr
  // pipe.
  int *StderrPipe() { return stderr_pipe_; }

  // Consumes all the data output to stderr by the subprocess and writes it to
  // the given output stream.
  void ConsumeAllSubprocessOutput(std::ostream *stderr_stream);

 private:
  explicit PosixSpawnIORedirector(int stderr_pipe[], bool stdoutToStderr) {
    memcpy(stderr_pipe_, stderr_pipe, sizeof(int) * 2);

    posix_spawn_file_actions_init(&file_actions_);
    posix_spawn_file_actions_addclose(&file_actions_, stderr_pipe_[0]);
    if (stdoutToStderr) {
      posix_spawn_file_actions_adddup2(&file_actions_, stderr_pipe_[1],
                                       STDOUT_FILENO);
    }
    posix_spawn_file_actions_adddup2(&file_actions_, stderr_pipe_[1],
                                     STDERR_FILENO);
    posix_spawn_file_actions_addclose(&file_actions_, stderr_pipe_[1]);
  }

  // Closes a file descriptor only if it hasn't already been closed.
  void SafeClose(int *fd) {
    if (*fd >= 0) {
      close(*fd);
      *fd = -1;
    }
  }

  int stderr_pipe_[2];
  posix_spawn_file_actions_t file_actions_;
};

void PosixSpawnIORedirector::ConsumeAllSubprocessOutput(
    std::ostream *stderr_stream) {
  SafeClose(&stderr_pipe_[1]);

  char stderr_buffer[1024];
  pollfd stderr_poll = {stderr_pipe_[0], POLLIN};
  int status;
  while ((status = poll(&stderr_poll, 1, -1)) > 0) {
    if (stderr_poll.revents) {
      int bytes_read =
          read(stderr_pipe_[0], stderr_buffer, sizeof(stderr_buffer));
      if (bytes_read == 0) {
        break;
      }
      stderr_stream->write(stderr_buffer, bytes_read);
    }
  }
}

// Converts an array of string arguments to char *arguments.
// The first arg is reduced to its basename as per execve conventions.
// Note that the lifetime of the char* arguments in the returned array
// are controlled by the lifetime of the strings in args.
std::vector<const char *> ConvertToCArgs(const std::vector<std::string> &args) {
  std::vector<const char *> c_args;
  std::string filename = std::filesystem::path(args[0]).filename().string();
  c_args.push_back(&*std::next(args[0].rbegin(), filename.length() - 1));
  for (int i = 1; i < args.size(); i++) {
    c_args.push_back(args[i].c_str());
  }
  c_args.push_back(nullptr);
  return c_args;
}

}  // namespace

int RunSubProcess(const std::vector<std::string> &args,
                  std::map<std::string, std::string> *env,
                  std::ostream *stderr_stream, bool stdout_to_stderr) {
  std::vector<const char *> exec_argv = ConvertToCArgs(args);

  // Set up a pipe to redirect stderr from the child process so that we can
  // capture it and return it in the response message.
  std::unique_ptr<PosixSpawnIORedirector> redirector =
      PosixSpawnIORedirector::Create(stdout_to_stderr);
  if (!redirector) {
    (*stderr_stream) << "Error creating stderr pipe for child process.\n";
    return 254;
  }

  char **envp;
  std::vector<char *> new_environ;

  if (env) {
    // Copy the environment as an array of C strings, with guaranteed cleanup
    // below whenever we exit.
    for (const auto &[key, value] : *env) {
      std::string pair = key + "=" + value;
      char* c_str = new char[pair.length() + 1];
      std::strcpy(c_str, pair.c_str());
      new_environ.push_back(c_str);
    }

    new_environ.push_back(nullptr);
    envp = new_environ.data();
  } else {
    // If no environment was passed, use the current process's verbatim.
    envp = environ;
  }

  pid_t pid;
  int status =
      posix_spawn(&pid, args[0].c_str(), redirector->PosixSpawnFileActions(),
                  nullptr, const_cast<char **>(exec_argv.data()), envp);
  redirector->ConsumeAllSubprocessOutput(stderr_stream);

  for (char* envp : new_environ) {
    if (envp) {
      delete[] envp;
    }
  }

  if (status == 0) {
    int wait_status;
    do {
      wait_status = waitpid(pid, &status, 0);
    } while ((wait_status == -1) && (errno == EINTR));

    if (wait_status < 0) {
      (*stderr_stream) << "error: waiting on child process '" << args[0]
                       << "'. " << strerror(errno) << "\n";
      return wait_status;
    }

    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
      return WTERMSIG(status);
    }

    // Unhandled case, if we hit this we should handle it above.
    return 42;
  } else {
    (*stderr_stream) << "error: forking process failed '" << args[0] << "'. "
                     << strerror(status) << "\n";
    return status;
  }
}
#endif
