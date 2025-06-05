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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_PROTOSTREAM_OBJECTWRITER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_PROTOSTREAM_OBJECTWRITER_H_

#include <deque>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/stubs/bytestream.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/util/converter/datapiece.h"
#include "google/protobuf/util/converter/error_listener.h"
#include "google/protobuf/util/converter/proto_writer.h"
#include "google/protobuf/util/converter/structured_objectwriter.h"
#include "google/protobuf/util/converter/type_info.h"
#include "google/protobuf/util/type_resolver.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class ObjectLocationTracker;

// An ObjectWriter that can write protobuf bytes directly from writer events.
// This class supports all special types like Struct and Map. It uses
// the ProtoWriter class to write raw proto bytes.
//
// It also supports streaming.
class ProtoStreamObjectWriter : public ProtoWriter {
 public:
  // Options that control ProtoStreamObjectWriter class's behavior.
  struct Options {
    // Treats numeric inputs in google.protobuf.Struct as strings. Normally,
    // numeric values are returned in double field "number_value" of
    // google.protobuf.Struct. However, this can cause precision loss for
    // int64/uint64/double inputs. This option is provided for cases that want
    // to preserve number precision.
    //
    // TODO(skarvaje): Rename to struct_numbers_as_strings as it covers double
    // as well.
    bool struct_integers_as_strings;

    // Not treat unknown fields as an error. If there is an unknown fields,
    // just ignore it and continue to process the rest. Note that this doesn't
    // apply to unknown enum values.
    bool ignore_unknown_fields;

    // Ignore unknown enum values.
    bool ignore_unknown_enum_values;

    // If true, check if enum name in camel case or without underscore matches
    // the field name.
    bool use_lower_camel_for_enums;

    // If true, check if enum name in UPPER_CASE matches the field name.
    bool case_insensitive_enum_parsing;

    // If true, skips rendering the map entry if map value is null unless the
    // value type is google.protobuf.NullType.
    bool ignore_null_value_map_entry;

    // If true, accepts repeated key/value pair for a map proto field.
    bool use_legacy_json_map_format;

    // If true, disable implicitly creating message list.
    bool disable_implicit_message_list;

    // If true, suppress the error of implicitly creating message list when it
    // is disabled.
    bool suppress_implicit_message_list_error;

    // If true, disable implicitly creating scalar list.
    bool disable_implicit_scalar_list;

    // If true, suppress the error of implicitly creating scalar list when it
    // is disabled.
    bool suppress_implicit_scalar_list_error;

    // If true, suppress the error of rendering scalar field if the source is an
    // object.
    bool suppress_object_to_scalar_error;

    // If true, use the json name in missing fields errors.
    bool use_json_name_in_missing_fields;

    Options()
        : struct_integers_as_strings(false),
          ignore_unknown_fields(false),
          ignore_unknown_enum_values(false),
          use_lower_camel_for_enums(false),
          case_insensitive_enum_parsing(false),
          ignore_null_value_map_entry(false),
          use_legacy_json_map_format(false),
          disable_implicit_message_list(false),
          suppress_implicit_message_list_error(false),
          disable_implicit_scalar_list(false),
          suppress_implicit_scalar_list_error(false),
          suppress_object_to_scalar_error(false),
          use_json_name_in_missing_fields(false) {}

    // Default instance of Options with all options set to defaults.
    static const Options& Defaults() {
      static Options defaults;
      return defaults;
    }
  };

  // Constructor. Does not take ownership of any parameter passed in.
  ProtoStreamObjectWriter(TypeResolver* type_resolver,
                          const google::protobuf::Type& type,
                          strings::ByteSink* output, ErrorListener* listener,
                          const ProtoStreamObjectWriter::Options& options =
                              ProtoStreamObjectWriter::Options::Defaults());
  ProtoStreamObjectWriter() = delete;
  ProtoStreamObjectWriter(const ProtoStreamObjectWriter&) = delete;
  ProtoStreamObjectWriter& operator=(const ProtoStreamObjectWriter&) = delete;
  ~ProtoStreamObjectWriter() override;

  // ObjectWriter methods.
  ProtoStreamObjectWriter* StartObject(absl::string_view name) override;
  ProtoStreamObjectWriter* EndObject() override;
  ProtoStreamObjectWriter* StartList(absl::string_view name) override;
  ProtoStreamObjectWriter* EndList() override;

  // Renders a DataPiece 'value' into a field whose wire type is determined
  // from the given field 'name'.
  ProtoStreamObjectWriter* RenderDataPiece(absl::string_view name,
                                           const DataPiece& data) override;

 protected:
  // Function that renders a well known type with modified behavior.
  typedef absl::Status (*TypeRenderer)(ProtoStreamObjectWriter*,
                                       const DataPiece&);

  // Handles writing Anys out using nested object writers and the like.
  class AnyWriter {
   public:
    explicit AnyWriter(ProtoStreamObjectWriter* parent);
    ~AnyWriter();

    // Passes a StartObject call through to the Any writer.
    void StartObject(absl::string_view name);

    // Passes an EndObject call through to the Any. Returns true if the any
    // handled the EndObject call, false if the Any is now all done and is no
    // longer needed.
    bool EndObject();

    // Passes a StartList call through to the Any writer.
    void StartList(absl::string_view name);

    // Passes an EndList call through to the Any writer.
    void EndList();

    // Renders a data piece on the any.
    void RenderDataPiece(absl::string_view name, const DataPiece& value);

   private:
    // Before the "@type" field is encountered, we store all incoming data
    // into this Event struct and replay them after we get the "@type" field.
    class Event {
     public:
      enum Type {
        START_OBJECT = 0,
        END_OBJECT = 1,
        START_LIST = 2,
        END_LIST = 3,
        RENDER_DATA_PIECE = 4,
      };

      // Constructor for END_OBJECT and END_LIST events.
      explicit Event(Type type) : type_(type), value_(DataPiece::NullData()) {}

      // Constructor for START_OBJECT and START_LIST events.
      explicit Event(Type type, absl::string_view name)
          : type_(type), name_(name), value_(DataPiece::NullData()) {}

      // Constructor for RENDER_DATA_PIECE events.
      explicit Event(absl::string_view name, const DataPiece& value)
          : type_(RENDER_DATA_PIECE), name_(name), value_(value) {
        DeepCopy();
      }

      Event(const Event& other)
          : type_(other.type_), name_(other.name_), value_(other.value_) {
        DeepCopy();
      }

      Event& operator=(const Event& other) {
        type_ = other.type_;
        name_ = other.name_;
        value_ = other.value_;
        DeepCopy();
        return *this;
      }

      void Replay(AnyWriter* writer) const;

     private:
      void DeepCopy();

      Type type_;
      std::string name_;
      DataPiece value_;
      std::string value_storage_;
    };

    // Handles starting up the any once we have a type.
    void StartAny(const DataPiece& value);

    // Writes the Any out to the parent writer in its serialized form.
    void WriteAny();

    // The parent of this writer, needed for various bits such as type info and
    // the listeners.
    ProtoStreamObjectWriter* parent_;

    // The nested object writer, used to write events.
    std::unique_ptr<ProtoStreamObjectWriter> ow_;

    // The type_url_ that this Any represents.
    std::string type_url_;

    // Whether this any is invalid. This allows us to only report an invalid
    // Any message a single time rather than every time we get a nested field.
    bool invalid_;

    // The output data and wrapping ByteSink.
    std::string data_;
    strings::StringByteSink output_;

    // The depth within the Any, so we can track when we're done.
    int depth_;

    // True if the type is a well-known type. Well-known types in Any
    // has a special formatting:
    // {
    //   "@type": "type.googleapis.com/google.protobuf.XXX",
    //   "value": <JSON representation of the type>,
    // }
    bool is_well_known_type_;
    TypeRenderer* well_known_type_render_;

    // Store data before the "@type" field.
    std::vector<Event> uninterpreted_events_;
  };

  // Represents an item in a stack of items used to keep state between
  // ObjectWrier events.
  class Item : public BaseElement {
   public:
    // Indicates the type of item.
    enum ItemType {
      MESSAGE,  // Simple message
      MAP,      // Proto3 map type
      ANY,      // Proto3 Any type
    };

    // Constructor for the root item.
    Item(ProtoStreamObjectWriter* enclosing, ItemType item_type,
         bool is_placeholder, bool is_list);

    // Constructor for a field of a message.
    Item(Item* parent, ItemType item_type, bool is_placeholder, bool is_list);
    Item() = delete;
    Item(const Item&) = delete;
    Item& operator=(const Item&) = delete;

    ~Item() override {}

    // These functions return true if the element type is corresponding to the
    // type in function name.
    bool IsMap() { return item_type_ == MAP; }
    bool IsAny() { return item_type_ == ANY; }

    AnyWriter* any() const { return any_.get(); }

    Item* parent() const override {
      return static_cast<Item*>(BaseElement::parent());
    }

    // Inserts map key into hash set if and only if the key did NOT already
    // exist in hash set.
    // The hash set (map_keys_) is ONLY used to keep track of map keys.
    // Return true if insert successfully; returns false if the map key was
    // already present.
    bool InsertMapKeyIfNotPresent(absl::string_view map_key);

    bool is_placeholder() const { return is_placeholder_; }
    bool is_list() const { return is_list_; }

   private:
    // Used for access to variables of the enclosing instance of
    // ProtoStreamObjectWriter.
    ProtoStreamObjectWriter* ow_;

    // A writer for Any objects, handles all Any-related nonsense.
    std::unique_ptr<AnyWriter> any_;

    // The type of this element, see enum for permissible types.
    ItemType item_type_;

    // Set of map keys already seen for the type_. Used to validate incoming
    // messages so no map key appears more than once.
    absl::flat_hash_set<std::string> map_keys_;

    // Conveys whether this Item is a placeholder or not. Placeholder items are
    // pushed to stack to account for special types.
    bool is_placeholder_;

    // Conveys whether this Item is a list or not. This is used to send
    // StartList or EndList calls to underlying ObjectWriter.
    bool is_list_;
  };

  ProtoStreamObjectWriter(const TypeInfo* typeinfo,
                          const google::protobuf::Type& type,
                          strings::ByteSink* output, ErrorListener* listener);

  ProtoStreamObjectWriter(const TypeInfo* typeinfo,
                          const google::protobuf::Type& type,
                          strings::ByteSink* output, ErrorListener* listener,
                          const ProtoStreamObjectWriter::Options& options);

  // Returns true if the field is a map.
  inline bool IsMap(const google::protobuf::Field& field);

  // Returns true if the field is an any.
  inline bool IsAny(const google::protobuf::Field& field);

  // Returns true if the field is google.protobuf.Struct.
  inline bool IsStruct(const google::protobuf::Field& field);

  // Returns true if the field is google.protobuf.Value.
  inline bool IsStructValue(const google::protobuf::Field& field);

  // Returns true if the field is google.protobuf.ListValue.
  inline bool IsStructListValue(const google::protobuf::Field& field);

  // Renders google.protobuf.Value in struct.proto. It picks the right oneof
  // type based on value's type.
  static absl::Status RenderStructValue(ProtoStreamObjectWriter* ow,
                                        const DataPiece& data);

  // Renders google.protobuf.Timestamp value.
  static absl::Status RenderTimestamp(ProtoStreamObjectWriter* ow,
                                      const DataPiece& data);

  // Renders google.protobuf.FieldMask value.
  static absl::Status RenderFieldMask(ProtoStreamObjectWriter* ow,
                                      const DataPiece& data);

  // Renders google.protobuf.Duration value.
  static absl::Status RenderDuration(ProtoStreamObjectWriter* ow,
                                     const DataPiece& data);

  // Renders wrapper message types for primitive types in
  // google/protobuf/wrappers.proto.
  static absl::Status RenderWrapperType(ProtoStreamObjectWriter* ow,
                                        const DataPiece& data);

  static void InitRendererMap();
  static void DeleteRendererMap();
  static TypeRenderer* FindTypeRenderer(const std::string& type_url);

  // Returns true if the map key for type_ is not duplicated key.
  // If map key is duplicated key, this function returns false.
  // Note that caller should make sure that the current proto element (current_)
  // is of element type MAP or STRUCT_MAP.
  // It also calls the appropriate error callback and unnormalzied_name is used
  // for error string.
  bool ValidMapKey(absl::string_view unnormalized_name);

  // Pushes an item on to the stack. Also calls either StartObject or StartList
  // on the underlying ObjectWriter depending on whether is_list is false or
  // not.
  // is_placeholder conveys whether the item is a placeholder item or not.
  // Placeholder items are pushed when adding auxiliary types' StartObject or
  // StartList calls.
  void Push(absl::string_view name, Item::ItemType item_type,
            bool is_placeholder, bool is_list);

  // Pops items from the stack. All placeholder items are popped until a
  // non-placeholder item is found.
  void Pop();

  // Pops one element from the stack. Calls EndObject() or EndList() on the
  // underlying ObjectWriter depending on the value of is_list_.
  void PopOneElement();

 private:
  // Helper functions to create the map and find functions responsible for
  // rendering well known types, keyed by type URL.
  static absl::flat_hash_map<std::string, TypeRenderer>* renderers_;

  // Variables for describing the structure of the input tree:
  // master_type_: descriptor for the whole protobuf message.
  const google::protobuf::Type& master_type_;

  // The current element, variable for internal state processing.
  std::unique_ptr<Item> current_;

  // Reference to the options that control this class's behavior.
  const ProtoStreamObjectWriter::Options options_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_PROTOSTREAM_OBJECTWRITER_H_
