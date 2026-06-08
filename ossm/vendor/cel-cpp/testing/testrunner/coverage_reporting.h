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

#ifndef THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_COVERAGE_REPORTING_H_
#define THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_COVERAGE_REPORTING_H_

#include "absl/strings/string_view.h"
#include "internal/testing.h"
#include "testing/testrunner/coverage_index.h"

namespace cel::test {
// A Google Test Environment that reports CEL coverage results in its TearDown
// phase.
//
// This class encapsulates the logic for calculating coverage statistics and
// logging them as test properties.
class CoverageReportingEnvironment : public testing::Environment {
 public:
  explicit CoverageReportingEnvironment(CoverageIndex& coverage_index)
      : coverage_index_(coverage_index) {};

  // Called by the Google Test framework after all tests have run.
  void TearDown() override;

 private:
  // Helper function to write the DOT graph to a test artifact file.
  void WriteDotGraphToArtifact(absl::string_view dot_graph);

  CoverageIndex& coverage_index_;
};
}  // namespace cel::test
#endif  // THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_COVERAGE_REPORTING_H_
