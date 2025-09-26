// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_TESTING_FILE_UTILS_H_
#define OCPDIAG_CORE_TESTING_FILE_UTILS_H_

#include "google/protobuf/message.h"
#include "absl/strings/string_view.h"

namespace ocpdiag::testutils {

// Generates a unique temporary file whose name includes the modifier string.
// Returns the path to this file.
std::string MkTempFileOrDie(absl::string_view modifier);

// Retrieves the full path of a test dependency file in the source tree.
std::string GetDataDependencyFilepath(absl::string_view file);

// Returns file contents from `file` in the source tree.
std::string GetDataDependencyFileContents(absl::string_view file);

// Writes proto in text format to the `file_full_path`.
void WriteProtoTextDebugFile(const google::protobuf::Message& msg,
                             const std::string& file_full_path);

}  // namespace ocpdiag::testutils

#endif  // OCPDIAG_CORE_TESTING_FILE_UTILS_H_
