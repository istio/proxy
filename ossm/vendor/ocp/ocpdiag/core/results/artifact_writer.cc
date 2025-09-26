// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/artifact_writer.h"

#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>
#include <thread>  //

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "google/protobuf/util/json_util.h"
#ifdef EXPAND_JSONL
#include "absl/strings/str_replace.h"
#endif
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ocpdiag/core/compat/status_converters.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "google/protobuf/util/time_util.h"
#include "riegeli/bytes/fd_writer.h"
#include "riegeli/records/records_metadata.pb.h"

namespace ocpdiag::results::internal {

constexpr absl::Duration kFlushFreq = absl::Minutes(1);

ArtifactWriter::ArtifactWriter(absl::string_view output_filepath,
                               std::ostream* output_stream,
                               bool flush_each_minute)
    : output_filepath_(output_filepath),
      output_stream_(output_stream),
      flush_each_minute_(flush_each_minute) {
  CHECK(!output_filepath.empty() || output_stream_ != nullptr)
      << "Must specify a valid filepath or output stream (or both) when "
         "creating an artifact writer.";
  SetupRecordWriter();
  SetupPeriodicFlush();
}

void ArtifactWriter::SetupRecordWriter() {
  if (output_filepath_.empty()) return;
  riegeli::RecordsMetadata metadata;
  riegeli::SetRecordType(*ocpdiag_results_v2_pb::OutputArtifact::GetDescriptor(),
                         metadata);

  absl::MutexLock lock(&mutex_);
  output_file_writer_.Reset(
      riegeli::FdWriter(output_filepath_),
      riegeli::RecordWriterBase::Options().set_metadata(std::move(metadata)));
  CHECK(output_file_writer_.ok())
      << "File writer error: " << output_file_writer_.status().ToString();
}

void ArtifactWriter::SetupPeriodicFlush() {
  if (output_filepath_.empty() || !flush_each_minute_) return;
  flush_thread_ = std::thread(&ArtifactWriter::FlushEveryMinute, this);
}

void ArtifactWriter::FlushEveryMinute() {
  absl::MutexLock lock(&mutex_);
  while (!mutex_.AwaitWithTimeout(absl::Condition(&stop_flush_routine_),
                                  kFlushFreq)) {
    FlushLocked();
  }
}

void ArtifactWriter::FlushLocked() {
  if (output_filepath_.empty()) return;
  output_file_writer_.Flush(riegeli::FlushType::kFromMachine);
}

void ArtifactWriter::Flush() {
  absl::MutexLock lock(&mutex_);
  return FlushLocked();
}

void ArtifactWriter::Write(
    const ocpdiag_results_v2_pb::TestRunArtifact& artifact) {
  ocpdiag_results_v2_pb::OutputArtifact proto;
  *proto.mutable_test_run_artifact() = artifact;
  Write(proto);
}

void ArtifactWriter::Write(
    const ocpdiag_results_v2_pb::TestStepArtifact& artifact) {
  ocpdiag_results_v2_pb::OutputArtifact proto;
  *proto.mutable_test_step_artifact() = artifact;
  Write(proto);
}

void ArtifactWriter::Write(
    const ocpdiag_results_v2_pb::SchemaVersion& artifact) {
  ocpdiag_results_v2_pb::OutputArtifact proto;
  *proto.mutable_schema_version() = artifact;
  Write(proto);
}

void ArtifactWriter::Write(ocpdiag_results_v2_pb::OutputArtifact& artifact) {
  absl::MutexLock lock(&mutex_);
  *artifact.mutable_timestamp() = google::protobuf::util::TimeUtil::GetCurrentTime();
  artifact.set_sequence_number(sequence_number_.Next());
  WriteToFile(artifact);
  WriteToStream(artifact);
}

void ArtifactWriter::WriteToFile(
    const ocpdiag_results_v2_pb::OutputArtifact& artifact) {
  if (output_filepath_.empty()) return;
  if (!output_file_writer_.WriteRecord(artifact)) {
    std::cerr << "Failed to write proto record to file: "
              << "\"" << artifact.DebugString() << "\"" << std::endl
              << "File writer error: "
              << output_file_writer_.status().ToString() << std::endl;
  }
}

void ArtifactWriter::WriteToStream(
    const ocpdiag_results_v2_pb::OutputArtifact& artifact) {
  if (output_stream_ == nullptr) return;
  google::protobuf::util::JsonPrintOptions opts;
  opts.always_print_primitive_fields = true;
#ifdef EXPAND_JSONL
  // Pretty print the JSON output
  opts.add_whitespace = true;
#endif

  std::string json;
  if (absl::Status status = AsAbslStatus(
          google::protobuf::util::MessageToJsonString(artifact, &json, opts));
      !status.ok()) {
    std::cerr << "Failed to serialize message: " << status.ToString()
              << std::endl;
    return;
  }

#ifdef EXPAND_JSONL
  // Escape all newline characters, otherwise parsers may fail.
  absl::StrReplaceAll({{R"(\\n)", R"(\n)"}}, &json);
  absl::StrReplaceAll({{R"(\n)", R"(\\n)"}}, &json);
#endif

  *output_stream_ << json << std::endl;
}

ArtifactWriter::~ArtifactWriter() {
  absl::ReleasableMutexLock releasable_lock(&mutex_);
  stop_flush_routine_ = true;
  if (output_filepath_.empty()) return;
  output_file_writer_.Close();
  if (!flush_each_minute_) return;
  releasable_lock.Release();
  flush_thread_.join();
}

}  // namespace ocpdiag::results::internal
