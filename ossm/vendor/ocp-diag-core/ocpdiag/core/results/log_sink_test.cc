// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/log_sink.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/log/log.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/output_receiver.h"

namespace ocpdiag::results::internal {

using ::testing::HasSubstr;

namespace {

TEST(LogSinkTest, InfoLogPropagatesToOutputProperly) {
  OutputReceiver receiver;
  std::unique_ptr<ArtifactWriter> writer = receiver.MakeArtifactWriter();
  LogSink sink(*writer);
  LOG(INFO).ToSinkOnly(&sink) << "test message";
  sink.Flush();

  EXPECT_EQ(receiver.GetOutputModel().test_run.pre_start_logs.size(), 1);
  EXPECT_THAT(receiver.GetOutputModel().test_run.pre_start_logs[0].message,
              HasSubstr("test message"));
  EXPECT_EQ(receiver.GetOutputModel().test_run.pre_start_logs[0].severity,
            LogSeverity::kInfo);
}

TEST(LogSinkTest, WarningLogPropagatesToOutputProperly) {
  OutputReceiver receiver;
  std::unique_ptr<ArtifactWriter> writer = receiver.MakeArtifactWriter();
  LogSink sink(*writer);
  LOG(WARNING).ToSinkOnly(&sink) << "warning";
  sink.Flush();

  EXPECT_EQ(receiver.GetOutputModel().test_run.pre_start_logs.size(), 1);
  EXPECT_THAT(receiver.GetOutputModel().test_run.pre_start_logs[0].message,
              HasSubstr("warning"));
  EXPECT_EQ(receiver.GetOutputModel().test_run.pre_start_logs[0].severity,
            LogSeverity::kWarning);
}

}  // namespace

}  // namespace ocpdiag::results::internal
