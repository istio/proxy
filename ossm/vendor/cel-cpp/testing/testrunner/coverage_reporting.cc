// Copyright 2025 Google LLC
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

#include "testing/testrunner/coverage_reporting.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "absl/log/absl_log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"
#include "testing/testrunner/coverage_index.h"

namespace cel::test {
void CoverageReportingEnvironment::TearDown() {
  CoverageIndex::CoverageReport coverage_report =
      coverage_index_.GetCoverageReport();
  testing::Test::RecordProperty("CEL Expression",
                                coverage_report.cel_expression);
  std::cout << "CEL Expression: " << coverage_report.cel_expression;
  if (coverage_report.nodes == 0) {
    testing::Test::RecordProperty("CEL Coverage", "No coverage stats found");
    std::cout << "CEL Coverage: " << "No coverage stats found";
    return;
  }

  // Log Node Coverage results
  double node_coverage = static_cast<double>(coverage_report.covered_nodes) /
                         static_cast<double>(coverage_report.nodes) * 100.0;
  std::string node_coverage_string =
      absl::StrFormat("%.2f%% (%d out of %d nodes covered)", node_coverage,
                      coverage_report.covered_nodes, coverage_report.nodes);
  testing::Test::RecordProperty("AST Node Coverage", node_coverage_string);
  std::cout << "AST Node Coverage: " << node_coverage_string;
  if (!coverage_report.unencountered_nodes.empty()) {
    testing::Test::RecordProperty(
        "Interesting Unencountered Nodes",
        absl::StrJoin(coverage_report.unencountered_nodes, "\n"));
    std::cout << "Interesting Unencountered Nodes: "
              << absl::StrJoin(coverage_report.unencountered_nodes, "\n");
  }

  // Log Branch Coverage results
  double branch_coverage = 0.0;
  if (coverage_report.branches > 0) {
    branch_coverage =
        static_cast<double>(coverage_report.covered_boolean_outcomes) /
        static_cast<double>(coverage_report.branches) * 100.0;
  }
  std::string branch_coverage_string = absl::StrFormat(
      "%.2f%% (%d out of %d branch outcomes covered)", branch_coverage,
      coverage_report.covered_boolean_outcomes, coverage_report.branches);
  testing::Test::RecordProperty("AST Branch Coverage", branch_coverage_string);
  std::cout << "AST Branch Coverage: " << branch_coverage_string;
  if (!coverage_report.unencountered_branches.empty()) {
    testing::Test::RecordProperty(
        "Interesting Unencountered Branch Paths",
        absl::StrJoin(coverage_report.unencountered_branches, "\n"));
    std::cout << "Interesting Unencountered Branch Paths: "
              << absl::StrJoin(coverage_report.unencountered_branches,
                               "\n");
  }
  if (!coverage_report.dot_graph.empty()) {
    WriteDotGraphToArtifact(coverage_report.dot_graph);
  }
}

void CoverageReportingEnvironment::WriteDotGraphToArtifact(
    absl::string_view dot_graph) {
  // Save DOT graph to file in TEST_UNDECLARED_OUTPUTS_DIR or default dir
  const char* outputs_dir_env = std::getenv("TEST_UNDECLARED_OUTPUTS_DIR");
  // For non-Bazel/Blaze users, we write to a subdirectory under the current
  // working directory.
  // NOMUTANTS --cel_artifacts is for non-Bazel/Blaze users only so not
  // needed to test in our case.
  std::string outputs_dir =
      (outputs_dir_env == nullptr) ? "cel_artifacts" : outputs_dir_env;
  std::string coverage_dir = absl::StrCat(outputs_dir, "/cel_test_coverage");
  // Creates the directory to store CEL test coverage artifacts.
  // The second argument, `0755`, sets the directory's permissions in octal
  // format, which is a standard for file system operations. It grants:
  //   - Owner: read, write, and execute permissions (7 = 4+2+1).
  //   - Group: read and execute permissions (5 = 4+1).
  //   - Others: read and execute permissions (5 = 4+1).
  // This gives the owner full control while allowing other users to access
  // the generated artifacts.
  int mkdir_result = mkdir(coverage_dir.c_str(), 0755);
  // If mkdir fails, it sets the global 'errno' variable to an error code
  // indicating the reason. We check this code to specifically ignore the
  // EEXIST error, which just means the directory already exists (this is not
  // a real failure we need to warn about).
  if (mkdir_result == 0 || errno == EEXIST) {
    std::string graph_path = absl::StrCat(coverage_dir, "/coverage_graph.txt");
    std::ofstream out(graph_path);
    if (out.is_open()) {
      out << dot_graph;
      out.close();
    } else {
      ABSL_LOG(WARNING) << "Failed to open file for writing: " << graph_path;
    }
  } else {
    ABSL_LOG(WARNING) << "Failed to create directory: " << coverage_dir
                      << " (reason: " << strerror(errno) << ")";
  }
}
}  // namespace cel::test
