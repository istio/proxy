// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_LIB_RESULTS_INTERNAL_LOGGING_H_
#define OCPDIAG_LIB_RESULTS_INTERNAL_LOGGING_H_

#include <ostream>
#include <thread>  //

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/int_incrementer.h"
#include "riegeli/base/object.h"
#include "riegeli/bytes/fd_writer.h"
#include "riegeli/records/record_writer.h"

namespace ocpdiag::results::internal {

// Writes test output to file in a compressed binary format, an output stream in
// JSONL format, or both.
class ArtifactWriter {
 public:
  ArtifactWriter(absl::string_view output_filepath,
                 std::ostream* output_stream = nullptr,
                 bool flush_each_minute = true);
  ~ArtifactWriter();

  // Flushes the file buffer, if any
  void Flush() ABSL_LOCKS_EXCLUDED(mutex_);

  // Write the artifact to the output file
  void Write(const ocpdiag_results_v2_pb::TestRunArtifact& artifact);
  void Write(const ocpdiag_results_v2_pb::TestStepArtifact& artifact);
  void Write(const ocpdiag_results_v2_pb::SchemaVersion& artifact);

 private:
  void SetupRecordWriter();
  void SetupPeriodicFlush();
  void FlushEveryMinute();
  bool GetRunFlushRoutine();

  void FlushLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void Write(ocpdiag_results_v2_pb::OutputArtifact& artifact);
  void WriteToFile(const ocpdiag_results_v2_pb::OutputArtifact& artifact)
      ABSL_SHARED_LOCKS_REQUIRED(&mutex_);
  void WriteToStream(const ocpdiag_results_v2_pb::OutputArtifact& artifact)
      ABSL_SHARED_LOCKS_REQUIRED(&mutex_);

  absl::Mutex mutex_;
  absl::string_view output_filepath_;
  std::ostream* output_stream_ ABSL_GUARDED_BY(mutex_);
  bool flush_each_minute_ = true;
  riegeli::RecordWriter<riegeli::FdWriter<>> output_file_writer_
      ABSL_GUARDED_BY(mutex_){riegeli::kClosed};
  bool stop_flush_routine_ ABSL_GUARDED_BY(mutex_) = false;
  std::thread flush_thread_;
  IntIncrementer sequence_number_;
};

}  // namespace ocpdiag::results::internal

#endif  // OCPDIAG_LIB_RESULTS_INTERNAL_LOGGING_H_
