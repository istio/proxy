// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/absl_check.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"

ABSL_FLAG(std::string, in, "", "");
ABSL_FLAG(std::string, out, "", "");

namespace {

std::vector<uint8_t> ReadFile(const std::string& path) {
  ABSL_CHECK(!path.empty()) << "--in is required";
  std::ifstream file(path);
  ABSL_CHECK(file.is_open()) << path;
  file.seekg(0, file.end);
  ABSL_CHECK(file.good());
  size_t size = static_cast<size_t>(file.tellg());
  file.seekg(0, file.beg);
  ABSL_CHECK(file.good());
  std::vector<uint8_t> buffer;
  buffer.resize(size);
  file.read(reinterpret_cast<char*>(buffer.data()), size);
  ABSL_CHECK(file.good());
  return buffer;
}

void WriteFile(const std::string& path, absl::Span<const char> data) {
  ABSL_CHECK(!path.empty()) << "--out is required";
  std::ofstream file(path);
  ABSL_CHECK(file.is_open()) << path;
  file.write(data.data(), data.size());
  ABSL_CHECK(file.good());
  file.flush();
  ABSL_CHECK(file.good());
}

}  // namespace

int main(int argc, char** argv) {
  {
    auto args = absl::ParseCommandLine(argc, argv);
    ABSL_CHECK(args.empty() || args.size() == 1)
        << "unexpected positional args: " << absl::StrJoin(args, ", ");
  }
  absl::InitializeLog();

  auto in_buffer = ReadFile(absl::GetFlag(FLAGS_in));
  std::string out_buffer;
  out_buffer.reserve(in_buffer.size() * 6);
  for (const auto& in_byte : in_buffer) {
    absl::StrAppend(&out_buffer, "0x",
                    absl::Hex(in_byte, absl::PadSpec::kZeroPad2), ", ");
  }
  if (!in_buffer.empty()) {
    // Replace last space with newline.
    out_buffer.back() = '\n';
  }
  WriteFile(absl::GetFlag(FLAGS_out), out_buffer);

  return EXIT_SUCCESS;
}
