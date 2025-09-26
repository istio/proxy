#pragma once

#include <stdio.h>

#include <fstream>
#include <string>

namespace test_common {

class TmpFile {
 public:
  TmpFile() : tmp_file_(std::tmpnam(nullptr)) {}
  ~TmpFile() { std::remove(tmp_file_.c_str()); }

  std::string GetTmpfileName() { return tmp_file_; }

  std::string GetFileContent() {
    std::ifstream file(tmp_file_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
  }

 private:
  std::string tmp_file_;
};

}  // namespace test_common
