// Copyright 2020 The Bazel Authors. All rights reserved.
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

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>  // NOLINT
#include <unordered_set>

using std::ifstream;
using std::regex;
using std::string;
using std::unordered_set;
using std::vector;

const regex libRegex = regex(".*\\.a$");
const regex noArgFlags =
    regex("-static|-s|-a|-c|-L|-T|-D|-v|-no_warning_for_no_symbols");
const regex singleArgFlags = regex("-arch_only|-syslibroot|-o");

string getBasename(const string &path) {
  // Assumes we're on an OS with "/" as the path separator
  auto idx = path.find_last_of("/");
  if (idx == string::npos) {
    return path;
  }
  return path.substr(idx + 1);
}

vector<string> readFile(const string path) {
  vector<string> lines;
  ifstream file(path);
  string line;
  while (std::getline(file, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }

  return lines;
}

unordered_set<string> parseArgs(vector<string> args) {
  unordered_set<string> basenames;
  for (auto it = args.begin(); it != args.end(); ++it) {
    const string arg = *it;
    if (arg == "-filelist") {
      ++it;
      ifstream list(*it);
      for (string line; getline(list, line);) {
        const string basename = getBasename(line);
        const auto pair = basenames.insert(basename);
        if (!pair.second) {
          exit(EXIT_FAILURE);
        }
      }
      list.close();
    } else if (arg[0] == '@') {
      string paramsFilePath(arg.substr(1));
      auto newBasenames = parseArgs(readFile(paramsFilePath));
      for (auto newBasename : newBasenames) {
        const auto pair = basenames.insert(newBasename);
        if (!pair.second) {
          exit(EXIT_FAILURE);
        }
      }
    } else if (regex_match(arg, noArgFlags)) {
    } else if (regex_match(arg, singleArgFlags)) {
      ++it;
    } else if (arg[0] == '-') {
      exit(EXIT_FAILURE);
      // Unrecognized flag, let the wrapper deal with it, any flags added to
      // libtool.sh should also be added here.
    } else if (regex_match(arg, libRegex)) {
      // Archive inputs can remain untouched, as they come from other targets.
    } else {
      const string basename = getBasename(arg);
      const auto pair = basenames.insert(basename);
      if (!pair.second) {
        exit(EXIT_FAILURE);
      }
    }
  }

  return basenames;
}

// Returns 0 if there are no duplicate basenames in the object files (via
// -filelist, params files, and shell args), 1 otherwise
int main(int argc, const char *argv[]) {
  vector<string> args;
  // Set i to 1 to skip executable path
  for (int i = 1; argv[i] != nullptr; i++) {
    args.push_back(argv[i]);
  }
  parseArgs(args);
  return EXIT_SUCCESS;
}
