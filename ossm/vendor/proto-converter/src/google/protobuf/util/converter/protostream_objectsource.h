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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_PROTOSTREAM_OBJECTSOURCE_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_PROTOSTREAM_OBJECTSOURCE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <stack>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/util/converter/object_source.h"
#include "google/protobuf/util/converter/object_writer.h"
#include "google/protobuf/util/converter/type_info.h"
#include "google/protobuf/util/type_resolver.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class TypeInfo;

// An ObjectSource that can parse a stream of bytes as a protocol buffer.
// Its WriteTo() method can be given an ObjectWriter.
// This implementation uses a google.protobuf.Type for tag and name lookup.
// The field names are converted into lower camel-case when writing to the
// ObjectWriter.
//
// Sample usage: (suppose input is: string proto)
//   ArrayInputStream arr_stream(proto.data(), proto.size());
//   CodedInputStream in_stream(&arr_stream);
//   ProtoStreamObjectSource os(&in_stream, /*ServiceTypeInfo*/ typeinfo,
//                              <your message google::protobuf::Type>);
//
//   Status status = os.WriteTo(<some ObjectWriter>);
class ProtoStreamObjectSource : public ObjectSource {
 public:
  struct RenderOptions {
    // Sets whether or not to use lowerCamelCase casing for enum values. If set
    // to false, enum values are output without any case conversions.
    //
    // For example, if we have an enum:
    // enum Type {
    //   ACTION_AND_ADVENTURE = 1;
    // }
    // Type type = 20;
    //
    // And this option is set to true. Then the rendered "type" field will have
    // the string "actionAndAdventure".
    // {
    //   ...
    //   "type": "actionAndAdventure",
    //   ...
    // }
    //
    // If set to false, the rendered "type" field will have the string
    // "ACTION_AND_ADVENTURE".
    // {
    //   ...
    //   "type": "ACTION_AND_ADVENTURE",
    //   ...
    // }
    bool use_lower_camel_for_enums = false;

    // Sets whether to always output enums as ints, by default this is off, and
    // enums are rendered as strings.
    bool use_ints_for_enums = false;

    // Whether to preserve proto field names
    bool preserve_proto_field_names = false;
  };

  ProtoStreamObjectSource(io::CodedInputStream* stream,
                          TypeResolver* type_resolver,
                          const google::protobuf::Type& type)
      : ProtoStreamObjectSource(stream, type_resolver, type, RenderOptions()) {}
  ProtoStreamObjectSource(io::CodedInputStream* stream,
                          TypeResolver* type_resolver,
                          const google::protobuf::Type& type,
                          const RenderOptions& render_options);
  ProtoStreamObjectSource() = delete;
  ProtoStreamObjectSource(const ProtoStreamObjectSource&) = delete;
  ProtoStreamObjectSource& operator=(const ProtoStreamObjectSource&) = delete;
  ~ProtoStreamObjectSource() override;

  absl::Status NamedWriteTo(absl::string_view name,
                            ObjectWriter* ow) const override;

  // Sets the max recursion depth of proto message to be deserialized. Proto
  // messages over this depth will fail to be deserialized.
  // Default value is 64.
  void set_max_recursion_depth(int max_depth) {
    max_recursion_depth_ = max_depth;
  }

 protected:
  // Writes a proto2 Message to the ObjectWriter. When the given end_tag is
  // found this method will complete, allowing it to be used for parsing both
  // nested messages (end with 0) and nested groups (end with group end tag).
  // The include_start_and_end parameter allows this method to be called when
  // already inside of an object, and skip calling StartObject and EndObject.
  virtual absl::Status WriteMessage(const google::protobuf::Type& type,
                                    absl::string_view name,
                                    const uint32_t end_tag,
                                    bool include_start_and_end,
                                    ObjectWriter* ow) const;

  // Renders a repeating field (packed or unpacked).  Returns the next tag after
  // reading all sequential repeating elements. The caller should use this tag
  // before reading more tags from the stream.
  virtual absl::StatusOr<uint32_t> RenderList(
      const google::protobuf::Field* field, absl::string_view name,
      uint32_t list_tag, ObjectWriter* ow) const;

  // Looks up a field and verify its consistency with wire type in tag.
  const google::protobuf::Field* FindAndVerifyFieldHelper(
      const google::protobuf::Type& type, uint32_t tag) const;

  // Looks up a field and verify its consistency with wire type in tag.
  virtual const google::protobuf::Field* FindAndVerifyField(
      const google::protobuf::Type& type, uint32_t tag) const;

  // Renders a field value to the ObjectWriter.
  virtual absl::Status RenderField(const google::protobuf::Field* field,
                                   absl::string_view field_name,
                                   ObjectWriter* ow) const;

  // Reads field value according to Field spec in 'field' and returns the read
  // value as string. This only works for primitive datatypes (no message
  // types).
  std::string ReadFieldValueAsString(
      const google::protobuf::Field& field) const;

  // Returns the input stream.
  io::CodedInputStream* stream() const { return stream_; }

 private:
  ProtoStreamObjectSource(io::CodedInputStream* stream,
                          const TypeInfo* typeinfo,
                          const google::protobuf::Type& type,
                          const RenderOptions& render_options);
  // Function that renders a well known type with a modified behavior.
  typedef absl::Status (*TypeRenderer)(const ProtoStreamObjectSource*,
                                       const google::protobuf::Type&,
                                       absl::string_view, ObjectWriter*);

  // TODO(skarvaje): Mark these methods as non-const as they modify internal
  // state (stream_).
  //
  // Renders a NWP map.
  // Returns the next tag after reading all map entries. The caller should use
  // this tag before reading more tags from the stream.
  absl::StatusOr<uint32_t> RenderMap(const google::protobuf::Field* field,
                                     absl::string_view name, uint32_t list_tag,
                                     ObjectWriter* ow) const;

  // Renders a packed repeating field. A packed field is stored as:
  // {tag length item1 item2 item3} instead of the less efficient
  // {tag item1 tag item2 tag item3}.
  absl::Status RenderPacked(const google::protobuf::Field* field,
                            ObjectWriter* ow) const;

  // Renders a google.protobuf.Timestamp value to ObjectWriter
  static absl::Status RenderTimestamp(const ProtoStreamObjectSource* os,
                                      const google::protobuf::Type& type,
                                      absl::string_view name, ObjectWriter* ow);

  // Renders a google.protobuf.Duration value to ObjectWriter
  static absl::Status RenderDuration(const ProtoStreamObjectSource* os,
                                     const google::protobuf::Type& type,
                                     absl::string_view name, ObjectWriter* ow);

  // Following RenderTYPE functions render well known types in
  // google/protobuf/wrappers.proto corresponding to TYPE.
  static absl::Status RenderDouble(const ProtoStreamObjectSource* os,
                                   const google::protobuf::Type& type,
                                   absl::string_view name, ObjectWriter* ow);
  static absl::Status RenderFloat(const ProtoStreamObjectSource* os,
                                  const google::protobuf::Type& type,
                                  absl::string_view name, ObjectWriter* ow);
  static absl::Status RenderInt64(const ProtoStreamObjectSource* os,
                                  const google::protobuf::Type& type,
                                  absl::string_view name, ObjectWriter* ow);
  static absl::Status RenderUInt64(const ProtoStreamObjectSource* os,
                                   const google::protobuf::Type& type,
                                   absl::string_view name, ObjectWriter* ow);
  static absl::Status RenderInt32(const ProtoStreamObjectSource* os,
                                  const google::protobuf::Type& type,
                                  absl::string_view name, ObjectWriter* ow);
  static absl::Status RenderUInt32(const ProtoStreamObjectSource* os,
                                   const google::protobuf::Type& type,
                                   absl::string_view name, ObjectWriter* ow);
  static absl::Status RenderBool(const ProtoStreamObjectSource* os,
                                 const google::protobuf::Type& type,
                                 absl::string_view name, ObjectWriter* ow);
  static absl::Status RenderString(const ProtoStreamObjectSource* os,
                                   const google::protobuf::Type& type,
                                   absl::string_view name, ObjectWriter* ow);
  static absl::Status RenderBytes(const ProtoStreamObjectSource* os,
                                  const google::protobuf::Type& type,
                                  absl::string_view name, ObjectWriter* ow);

  // Renders a google.protobuf.Struct to ObjectWriter.
  static absl::Status RenderStruct(const ProtoStreamObjectSource* os,
                                   const google::protobuf::Type& type,
                                   absl::string_view name, ObjectWriter* ow);

  // Helper to render google.protobuf.Struct's Value fields to ObjectWriter.
  static absl::Status RenderStructValue(const ProtoStreamObjectSource* os,
                                        const google::protobuf::Type& type,
                                        absl::string_view name,
                                        ObjectWriter* ow);

  // Helper to render google.protobuf.Struct's ListValue fields to ObjectWriter.
  static absl::Status RenderStructListValue(const ProtoStreamObjectSource* os,
                                            const google::protobuf::Type& type,
                                            absl::string_view name,
                                            ObjectWriter* ow);

  // Render the "Any" type.
  static absl::Status RenderAny(const ProtoStreamObjectSource* os,
                                const google::protobuf::Type& type,
                                absl::string_view name, ObjectWriter* ow);

  // Render the "FieldMask" type.
  static absl::Status RenderFieldMask(const ProtoStreamObjectSource* os,
                                      const google::protobuf::Type& type,
                                      absl::string_view name, ObjectWriter* ow);

  static absl::flat_hash_map<std::string, TypeRenderer>* renderers_;
  static void InitRendererMap();
  static void DeleteRendererMap();
  static TypeRenderer* FindTypeRenderer(const std::string& type_url);

  // Same as above but renders all non-message field types. Callers don't call
  // this function directly. They just use RenderField.
  absl::Status RenderNonMessageField(const google::protobuf::Field* field,
                                     absl::string_view field_name,
                                     ObjectWriter* ow) const;

  // Utility function to detect proto maps. The 'field' MUST be repeated.
  bool IsMap(const google::protobuf::Field& field) const;

  // Utility to read int64 and int32 values from a message type in stream_.
  // Used for reading google.protobuf.Timestamp and Duration messages.
  std::pair<int64_t, int32_t> ReadSecondsAndNanos(
      const google::protobuf::Type& type) const;

  // Helper function to check recursion depth and increment it. It will return
  // OkStatus() if the current depth is allowed. Otherwise an error is returned.
  // type_name and field_name are used for error reporting.
  absl::Status IncrementRecursionDepth(absl::string_view type_name,
                                       absl::string_view field_name) const;

  // Input stream to read from. Ownership rests with the caller.
  mutable io::CodedInputStream* stream_;

  // Type information for all the types used in the descriptor. Used to find
  // google::protobuf::Type of nested messages/enums.
  const TypeInfo* typeinfo_;

  // Whether this class owns the typeinfo_ object. If true the typeinfo_ object
  // should be deleted in the destructor.
  bool own_typeinfo_;

  // google::protobuf::Type of the message source.
  const google::protobuf::Type& type_;

  const RenderOptions render_options_;

  // Tracks current recursion depth.
  mutable int recursion_depth_;

  // Maximum allowed recursion depth.
  int max_recursion_depth_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_PROTOSTREAM_OBJECTSOURCE_H_
