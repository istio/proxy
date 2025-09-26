/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_SOURCE_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_SOURCE_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class ObjectWriter;

// An ObjectSource is anything that can write to an ObjectWriter.
// Implementation of this interface typically provide constructors or
// factory methods to create an instance based on some source data, for
// example, a character stream, or protobuf.
//
// Derived classes could be thread-unsafe.
class ObjectSource {
 public:
  ObjectSource(const ObjectSource&) = delete;
  ObjectSource& operator=(const ObjectSource&) = delete;
  virtual ~ObjectSource() {}

  // Writes to the ObjectWriter
  virtual absl::Status WriteTo(ObjectWriter* ow) const {
    return NamedWriteTo("", ow);
  }

  // Writes to the ObjectWriter with a custom name for the message.
  // This is useful when you chain ObjectSource together by embedding one
  // within another.
  virtual absl::Status NamedWriteTo(absl::string_view name,
                                    ObjectWriter* ow) const = 0;

 protected:
  ObjectSource() {}
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_SOURCE_H_
