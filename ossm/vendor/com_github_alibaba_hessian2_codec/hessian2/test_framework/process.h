#pragma once

#include <assert.h>
#include <stdio.h>

#include <array>
#include <string>

namespace Hessian2 {

#if (defined WIN32) || (defined _WIN32)

#define popen _popen
#define pclose _pclose

#endif

class Process {
 public:
  Process() = default;
  ~Process() = default;

  bool write(const std::string& input) {
    assert(write_mode_);
    assert(pipe_);
    auto size = fwrite(input.data(), 1, input.size(), pipe_);
    if (size < input.size()) {
      pclose(pipe_);
      return false;
    }
    fflush(pipe_);
    pclose(pipe_);
    return true;
  }

  bool RunWithWriteMode(const std::string& command) {
    write_mode_ = true;
    pipe_ = popen(command.c_str(), "w");
    if (!pipe_) {
      return false;
    }
    return true;
  }

  bool Run(const std::string& command) {
    output_.clear();
    pipe_ = popen(command.c_str(), "r");
    if (!pipe_) {
      return false;
    }
    while (!std::feof(pipe_)) {
      char buf[129] = {0};
      auto size = std::fread(buf, 1, 128, pipe_);
      output_.append(buf, size);
    }
    pclose(pipe_);
    return true;
  }

  std::string Output() { return output_; }

 private:
  std::FILE* pipe_{nullptr};
  std::string output_;
  bool write_mode_{false};
};

#if (defined WIN32) || (defined _WIN32)

#undef popen
#undef pclose

#endif

}  // namespace Hessian2
