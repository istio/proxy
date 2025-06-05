/* Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef GRPC_TRANSCODING_PREFIX_WRITER_H_
#define GRPC_TRANSCODING_PREFIX_WRITER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "google/protobuf/util/converter/object_writer.h"

namespace google {
namespace grpc {

namespace transcoding {

// PrefixWriter is helper ObjectWriter implementation that for each incoming
// object
//  1) writes the given prefix path by starting objects to the output
//     ObjectWriter,
//  2) forwards the writer events for a single object,
//  3) unwinds the prefix, by closing objects in the reverse order.
//
// E.g.
//
//  PrefixWriter pw("A.B.C", out);
//  pw.StartObject("Root");
//  ...
//  pw.RenderString("x", "value");
//  ...
//  pw.EndObject("Root");
//
// is equivalent to
//
//  out.StartObject("Root");
//  out.StartObject("A");
//  out.StartObject("B");
//  out.StartObject("C");
//  ...
//  pw.RenderString("x", "value");
//  ...
//  out.EndObject("C");
//  out.EndObject("B");
//  out.EndObject("A");
//  out.EndObject("Root");
//
class PrefixWriter : public google::protobuf::util::converter::ObjectWriter {
 public:
  // prefix is a '.' delimited prefix path to be added
  PrefixWriter(const std::string& prefix,
               google::protobuf::util::converter::ObjectWriter* ow);

  // ObjectWriter methods.
  PrefixWriter* StartObject(absl::string_view name);
  PrefixWriter* EndObject();
  PrefixWriter* StartList(absl::string_view name);
  PrefixWriter* EndList();
  PrefixWriter* RenderBool(absl::string_view name, bool value);
  PrefixWriter* RenderInt32(absl::string_view name, int32_t value);
  PrefixWriter* RenderUint32(absl::string_view name, uint32_t value);
  PrefixWriter* RenderInt64(absl::string_view name, int64_t value);
  PrefixWriter* RenderUint64(absl::string_view name, uint64_t value);
  PrefixWriter* RenderDouble(absl::string_view name, double value);
  PrefixWriter* RenderFloat(absl::string_view name, float value);
  PrefixWriter* RenderString(absl::string_view name, absl::string_view value);
  PrefixWriter* RenderBytes(absl::string_view name, absl::string_view value);
  PrefixWriter* RenderNull(absl::string_view name);

 private:
  // Helper method to start the prefix and return the name to use for the value.
  absl::string_view StartPrefix(absl::string_view name);

  // Helper method to end the prefix.
  void EndPrefix();

  // The path prefix if the HTTP body maps to a nested message in the proto.
  std::vector<std::string> prefix_;

  // Tracks the depth within the output, so we know when to write the prefix
  // and when to close it off.
  int non_actionable_depth_;

  // The output object writer to forward the writer events.
  google::protobuf::util::converter::ObjectWriter* writer_;

  PrefixWriter(const PrefixWriter&) = delete;
  PrefixWriter& operator=(const PrefixWriter&) = delete;
};

}  // namespace transcoding

}  // namespace grpc
}  // namespace google

#endif  // GRPC_TRANSCODING_PREFIX_WRITER_H_
