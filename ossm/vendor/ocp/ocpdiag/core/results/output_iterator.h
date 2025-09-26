// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OUTPUT_ITERATOR_H_
#define OCPDIAG_CORE_RESULTS_OUTPUT_ITERATOR_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/log/check.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/data_model/proto_to_struct.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "riegeli/bytes/fd_reader.h"
#include "riegeli/records/record_reader.h"

namespace ocpdiag::results {

// Satisfies the interface for range-based for loops in C++, to allow you to
// iterate through OCPDiag test OutputArtifacts by pointing this class to the
// recordio OCPDiag output. It crashes if errors are encountered, so this is not
// suitable for production code. It is intended for unit tests only.
class OutputIterator {
 public:
  // Constructs a new iterator, pointing to the first OutputArtifact (if any).
  // If file_path is left unset, it will construct an invalid iterator.
  OutputIterator(std::optional<absl::string_view> file_path = std::nullopt) {
    if (!file_path.has_value()) return;
    reader_ = std::make_unique<riegeli::RecordReader<riegeli::FdReader<>>>(
        riegeli::FdReader{*file_path});
    ++(*this);  // advance ourselves so we always start on the first item
  }

  // Dereferences the iterator.
  OutputArtifact &operator*() { return output_; }
  OutputArtifact *operator->() { return &output_; }

  // Advances the iterator.
  OutputIterator &operator++() {
    ocpdiag_results_v2_pb::OutputArtifact output_proto;
    if (!reader_->ReadRecord(output_proto)) {
      CHECK_OK(reader_->status()) << "Failed while reading recordio";
      reader_.reset();
      return *this;
    }
    output_ = internal::ProtoToStruct(output_proto);
    return *this;
  }

  // The boolean operator can also be used to tell if the iterator still has
  // data left to consume.
  operator bool() const { return reader_ != nullptr; }

  // We can only compare valid iterator vs. invalid, but we can't tell the
  // difference between two valid iterators.
  bool operator!=(const OutputIterator &other) const {
    return (bool)*this != (bool)other;
  }

 private:
  std::unique_ptr<riegeli::RecordReader<riegeli::FdReader<>>> reader_;
  OutputArtifact output_;
};

// OutputContainer defines a container of OutputArtifacts that you can iterate
// through.
//
// Example:
//   for (const OutputArtifact& artifact : OutputContainer(path)) {...}
class OutputContainer {
 public:
  // The iterator allows this class to be used as a container in range-based for
  // loops.
  using const_iterator = OutputIterator;

  // The container will read from the given file_path.
  OutputContainer(absl::string_view file_path) : file_path_(file_path) {}

  absl::string_view file_path() const { return file_path_; }

  const_iterator begin() const { return OutputIterator(file_path_); }
  const_iterator end() const { return OutputIterator(); }

 private:
  std::string file_path_;
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OUTPUT_ITERATOR_H_
