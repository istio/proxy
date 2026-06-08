// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/examples/full_spec/full_spec.h"

#include "gtest/gtest.h"
#include "ocpdiag/core/results/output_receiver.h"

namespace full_spec {

using ::ocpdiag::results::OutputReceiver;

namespace {

// Since all of the information in this example is hardcoded, it is essentially
// a glorified text file, so we only verify the number of artifacts so that the
// diag is run by CI to catch any runtime errors.
TEST(FullSpecTest, FullSpecOutputHasExpectedArtifactNumber) {
  OutputReceiver receiver;
  FullSpec(receiver.MakeArtifactWriter()).ExecuteTest();

  int artifact_count = 0;
  for (auto unused : receiver.GetOutputContainer()) artifact_count++;
  EXPECT_EQ(artifact_count, 24);
}

}  // namespace

}  // namespace full_spec
