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

#ifndef BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_OUTPUT_FILE_MAP_H
#define BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_OUTPUT_FILE_MAP_H

#include <map>
#include <nlohmann/json.hpp>
#include <string>

// Supports loading and rewriting a `swiftc` output file map to support
// incremental compilation.
//
// See
// https://github.com/apple/swift/blob/master/docs/Driver.md#output-file-maps
// for more information on how the Swift driver uses this file.
class OutputFileMap {
 public:
  explicit OutputFileMap() {}

  // The in-memory JSON-based representation of the output file map.
  const nlohmann::json &json() const { return json_; }

  // A map containing expected output files that will be generated in the
  // incremental storage area. The key is the original object path; the
  // corresponding value is its location in the incremental storage area.
  const std::map<std::string, std::string> incremental_outputs() const {
    return incremental_outputs_;
  }

  // A map containing expected output files that will be generated in the
  // non-incremental storage area, but need to be copied back at the start of
  // the next compile. The key is the original object path; the corresponding
  // value is its location in the incremental storage area.
  const std::map<std::string, std::string> incremental_inputs() const {
    return incremental_inputs_;
  }

  // A list of output files that will be generated in the incremental storage
  // area, and need to be cleaned up if a corrupt module is detected.
  const std::vector<std::string> incremental_cleanup_outputs() const {
    return incremental_cleanup_outputs_;
  }

  // Reads the output file map from the JSON file at the given path, and updates
  // it to support incremental builds.
  void ReadFromPath(const std::string &path,
                    const std::string &emit_module_path,
                    const std::string &emit_objc_header_path);

  // Writes the output file map as JSON to the file at the given path.
  void WriteToPath(const std::string &path);

 private:
  // Modifies the output file map's JSON structure in-place to replace file
  // paths with equivalents in the incremental storage area.
  void UpdateForIncremental(const std::string &path,
                            const std::string &emit_module_path,
                            const std::string &emit_objc_header_path);

  nlohmann::json json_;
  std::map<std::string, std::string> incremental_outputs_;
  std::map<std::string, std::string> incremental_inputs_;
  std::vector<std::string> incremental_cleanup_outputs_;
};

#endif  // BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_OUTPUT_FILE_MAP_H_
