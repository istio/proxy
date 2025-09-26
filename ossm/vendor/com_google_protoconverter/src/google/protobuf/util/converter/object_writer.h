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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_WRITER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_WRITER_H_

#include <cstdint>

#include "absl/strings/string_view.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class DataPiece;

// An ObjectWriter is an interface for writing a stream of events
// representing objects and collections. Implementation of this
// interface can be used to write an object stream to an in-memory
// structure, protobufs, JSON, XML, or any other output format
// desired. The ObjectSource interface is typically used as the
// source of an object stream.
//
// See JsonObjectWriter for a sample implementation of ObjectWriter
// and its use.
//
// Derived classes could be thread-unsafe.
//
// TODO(xinb): seems like a prime candidate to apply the RAII paradigm
// and get rid the need to call EndXXX().
class ObjectWriter {
 public:
  ObjectWriter(const ObjectWriter&) = delete;
  ObjectWriter& operator=(const ObjectWriter&) = delete;
  virtual ~ObjectWriter() {}

  // Starts an object. If the name is empty, the object will not be named.
  virtual ObjectWriter* StartObject(absl::string_view name) = 0;

  // Ends an object.
  virtual ObjectWriter* EndObject() = 0;

  // Starts a list. If the name is empty, the list will not be named.
  virtual ObjectWriter* StartList(absl::string_view name) = 0;

  // Ends a list.
  virtual ObjectWriter* EndList() = 0;

  // Renders a boolean value.
  virtual ObjectWriter* RenderBool(absl::string_view name, bool value) = 0;

  // Renders an 32-bit integer value.
  virtual ObjectWriter* RenderInt32(absl::string_view name, int32_t value) = 0;

  // Renders an 32-bit unsigned integer value.
  virtual ObjectWriter* RenderUint32(absl::string_view name,
                                     uint32_t value) = 0;

  // Renders a 64-bit integer value.
  virtual ObjectWriter* RenderInt64(absl::string_view name, int64_t value) = 0;

  // Renders an 64-bit unsigned integer value.
  virtual ObjectWriter* RenderUint64(absl::string_view name,
                                     uint64_t value) = 0;

  // Renders a double value.
  virtual ObjectWriter* RenderDouble(absl::string_view name, double value) = 0;
  // Renders a float value.
  virtual ObjectWriter* RenderFloat(absl::string_view name, float value) = 0;

  // Renders a string value.
  virtual ObjectWriter* RenderString(absl::string_view name,
                                     absl::string_view value) = 0;

  // Renders a bytes value.
  virtual ObjectWriter* RenderBytes(absl::string_view name,
                                    absl::string_view value) = 0;

  // Renders a Null value.
  virtual ObjectWriter* RenderNull(absl::string_view name) = 0;

  // Renders a DataPiece object to a ObjectWriter.
  static void RenderDataPieceTo(const DataPiece& data, absl::string_view name,
                                ObjectWriter* ow);

  // Indicates whether this ObjectWriter has completed writing the root message,
  // usually this means writing of one complete object. Subclasses must override
  // this behavior appropriately.
  virtual bool done() { return false; }

  void set_use_strict_base64_decoding(bool value) {
    use_strict_base64_decoding_ = value;
  }

  bool use_strict_base64_decoding() const {
    return use_strict_base64_decoding_;
  }

 protected:
  ObjectWriter() : use_strict_base64_decoding_(true) {}

 private:
  // If set to true, we use the stricter version of base64 decoding for byte
  // fields by making sure decoded version encodes back to the original string.
  bool use_strict_base64_decoding_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_WRITER_H_
