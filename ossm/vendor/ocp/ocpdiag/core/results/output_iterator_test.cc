// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/output_iterator.h"

#include <string>

#include "gtest/gtest.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/testing/file_utils.h"
#include "ocpdiag/core/testing/parse_text_proto.h"
#include "riegeli/bytes/fd_writer.h"
#include "riegeli/records/record_writer.h"

namespace ocpdiag::results {
namespace {

class OutputIteratorTest : public ::testing::Test {
 protected:
  OutputIteratorTest()
      : filepath_(testutils::MkTempFileOrDie("output_iterator")) {
    ocpdiag_results_v2_pb::OutputArtifact artifact =
        testing::ParseTextProtoOrDie(R"pb(
          schema_version { major: 2 minor: 0 }
        )pb");
    riegeli::RecordWriter writer(riegeli::FdWriter<>{filepath_});
    for (int i = 0; i < num_protos_; ++i)
      CHECK(writer.WriteRecord(artifact)) << writer.status().message();
    writer.Close();
  }

  const int num_protos_ = 10;
  std::string filepath_;
};

TEST_F(OutputIteratorTest, IteratorWorksInRangeBasedForLoop) {
  int cnt = 0;
  OutputIterator iter(filepath_);
  OutputIterator end;
  for (; iter != end; ++iter) {
    // Make sure the * operator works.
    OutputArtifact unused_artifact = std::move(*iter);
    cnt++;
  }
  EXPECT_EQ(cnt, num_protos_);
}

TEST_F(OutputIteratorTest, IteratorBooleanOperatorLoopingFunctions) {
  // The boolean operator is important for the pybind wrapper, because the
  // container's end iterator is not available so this is how we check if the
  // iterator is valid.
  int cnt = 0;
  for (auto iter = OutputIterator(filepath_); iter; ++iter) cnt++;
  EXPECT_EQ(cnt, num_protos_);
}

TEST_F(OutputIteratorTest, ContainerWorksInRangeBasedForLoop) {
  int cnt = 0;
  OutputContainer container(filepath_);
  ASSERT_EQ(container.file_path(), filepath_);

  for (OutputArtifact artifact : container) cnt++;
  EXPECT_EQ(cnt, num_protos_);
}

TEST(OutputIteratorDeathTest, BadFilepathCausesDeath) {
  EXPECT_DEATH(OutputIterator(""), "");
  EXPECT_DEATH(OutputIterator("path-doesnt-exist"), "");
}

}  // namespace
}  // namespace ocpdiag::results
