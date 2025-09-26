// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_LOG_SINK_H_
#define OCPDIAG_CORE_RESULTS_OCP_LOG_SINK_H_

#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/results.pb.h"

namespace ocpdiag::results::internal {

// Custom ABSL LogSink that redirect the ABSL log to the global ArtifactWriter.
class LogSink : public absl::LogSink {
 public:
  LogSink(ArtifactWriter& writer) : writer_(writer) {}

  // Log function that directly logs the message with ArtifactWriter. This will
  // allow logging without TestRun or TestStep.
  void Send(const absl::LogEntry& entry) final {
    ocpdiag_results_v2_pb::TestRunArtifact run_proto;
    ocpdiag_results_v2_pb::Log* log_proto = run_proto.mutable_log();
    log_proto->set_message(std::string(entry.text_message()));
    log_proto->set_severity(
        ocpdiag_results_v2_pb::Log::Severity(entry.log_severity()));
    writer_.Write(run_proto);
  }

  // Flushes logs to the output file and / or stream targeted by the artifact
  // writer.
  void Flush() final { writer_.Flush(); }

 private:
  ArtifactWriter& writer_;
};

}  // namespace ocpdiag::results::internal

#endif  // OCPDIAG_CORE_RESULTS_OCP_LOG_SINK_H_
