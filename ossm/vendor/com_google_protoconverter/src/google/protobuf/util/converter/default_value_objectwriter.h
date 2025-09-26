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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_DEFAULT_VALUE_OBJECTWRITER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_DEFAULT_VALUE_OBJECTWRITER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <stack>
#include <vector>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/util/converter/datapiece.h"
#include "google/protobuf/util/converter/object_writer.h"
#include "google/protobuf/util/converter/type_info.h"
#include "google/protobuf/util/converter/utility.h"
#include "google/protobuf/util/type_resolver.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// An ObjectWriter that renders non-repeated primitive fields of proto messages
// with their default values. DefaultValueObjectWriter holds objects, lists and
// fields it receives in a tree structure and writes them out to another
// ObjectWriter when EndObject() is called on the root object. It also writes
// out all non-repeated primitive fields that haven't been explicitly rendered
// with their default values (0 for numbers, "" for strings, etc).
class DefaultValueObjectWriter : public ObjectWriter {
 public:
  // A Callback function to check whether a field needs to be scrubbed.
  //
  // Returns true if the field should not be present in the output. Returns
  // false otherwise.
  //
  // The 'path' parameter is a vector of path to the field from root. For
  // example: if a nested field "a.b.c" (b is the parent message field of c and
  // a is the parent message field of b), then the vector should contain { "a",
  // "b", "c" }.
  //
  // The Field* should point to the google::protobuf::Field of "c".
  typedef std::function<bool(
      const std::vector<std::string>& /*path of the field*/,
      const google::protobuf::Field* /*field*/)>
      FieldScrubCallBack;

  DefaultValueObjectWriter(TypeResolver* type_resolver,
                           const google::protobuf::Type& type,
                           ObjectWriter* ow);

  DefaultValueObjectWriter(const DefaultValueObjectWriter&) = delete;
  DefaultValueObjectWriter& operator=(const DefaultValueObjectWriter&) = delete;
  ~DefaultValueObjectWriter() override;

  // ObjectWriter methods.
  DefaultValueObjectWriter* StartObject(absl::string_view name) override;

  DefaultValueObjectWriter* EndObject() override;

  DefaultValueObjectWriter* StartList(absl::string_view name) override;

  DefaultValueObjectWriter* EndList() override;

  DefaultValueObjectWriter* RenderBool(absl::string_view name,
                                       bool value) override;

  DefaultValueObjectWriter* RenderInt32(absl::string_view name,
                                        int32_t value) override;

  DefaultValueObjectWriter* RenderUint32(absl::string_view name,
                                         uint32_t value) override;

  DefaultValueObjectWriter* RenderInt64(absl::string_view name,
                                        int64_t value) override;

  DefaultValueObjectWriter* RenderUint64(absl::string_view name,
                                         uint64_t value) override;

  DefaultValueObjectWriter* RenderDouble(absl::string_view name,
                                         double value) override;

  DefaultValueObjectWriter* RenderFloat(absl::string_view name,
                                        float value) override;

  DefaultValueObjectWriter* RenderString(absl::string_view name,
                                         absl::string_view value) override;
  DefaultValueObjectWriter* RenderBytes(absl::string_view name,
                                        absl::string_view value) override;

  DefaultValueObjectWriter* RenderNull(absl::string_view name) override;

  // Register the callback for scrubbing of fields.
  void RegisterFieldScrubCallBack(FieldScrubCallBack field_scrub_callback);

  // If set to true, empty lists are suppressed from output when default values
  // are written.
  void set_suppress_empty_list(bool value) { suppress_empty_list_ = value; }

  // If set to true, original proto field names are used
  void set_preserve_proto_field_names(bool value) {
    preserve_proto_field_names_ = value;
  }

  // If set to true, enums are rendered as ints from output when default values
  // are written.
  void set_print_enums_as_ints(bool value) { use_ints_for_enums_ = value; }

 protected:
  enum NodeKind {
    PRIMITIVE = 0,
    OBJECT = 1,
    LIST = 2,
    MAP = 3,
  };

  // "Node" represents a node in the tree that holds the input of
  // DefaultValueObjectWriter.
  class Node {
   public:
    Node(const std::string& name, const google::protobuf::Type* type,
         NodeKind kind, const DataPiece& data, bool is_placeholder,
         const std::vector<std::string>& path, bool suppress_empty_list,
         bool preserve_proto_field_names, bool use_ints_for_enums,
         FieldScrubCallBack field_scrub_callback);
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    virtual ~Node() {
      for (int i = 0; i < children_.size(); ++i) {
        delete children_[i];
      }
    }

    // Adds a child to this node. Takes ownership of this child.
    void AddChild(Node* child) { children_.push_back(child); }

    // Finds the child given its name.
    Node* FindChild(absl::string_view name);

    // Populates children of this Node based on its type. If there are already
    // children created, they will be merged to the result. Caller should pass
    // in TypeInfo for looking up types of the children.
    virtual void PopulateChildren(const TypeInfo* typeinfo);

    // If this node is a leaf (has data), writes the current node to the
    // ObjectWriter; if not, then recursively writes the children to the
    // ObjectWriter.
    virtual void WriteTo(ObjectWriter* ow);

    // Accessors
    const std::string& name() const { return name_; }

    const std::vector<std::string>& path() const { return path_; }

    const google::protobuf::Type* type() const { return type_; }

    void set_type(const google::protobuf::Type* type) { type_ = type; }

    NodeKind kind() const { return kind_; }

    int number_of_children() const { return children_.size(); }

    void set_data(const DataPiece& data) { data_ = data; }

    bool is_any() const { return is_any_; }

    void set_is_any(bool is_any) { is_any_ = is_any; }

    void set_is_placeholder(bool is_placeholder) {
      is_placeholder_ = is_placeholder;
    }

   protected:
    // Returns the Value Type of a map given the Type of the map entry and a
    // TypeInfo instance.
    const google::protobuf::Type* GetMapValueType(
        const google::protobuf::Type& found_type, const TypeInfo* typeinfo);

    // Calls WriteTo() on every child in children_.
    void WriteChildren(ObjectWriter* ow);

    // The name of this node.
    std::string name_;
    // google::protobuf::Type of this node. Owned by TypeInfo.
    const google::protobuf::Type* type_;
    // The kind of this node.
    NodeKind kind_;
    // Whether this is a node for "Any".
    bool is_any_;
    // The data of this node when it is a leaf node.
    DataPiece data_;
    // Children of this node.
    std::vector<Node*> children_;
    // Whether this node is a placeholder for an object or list automatically
    // generated when creating the parent node. Should be set to false after
    // the parent node's StartObject()/StartList() method is called with this
    // node's name.
    bool is_placeholder_;

    // Path of the field of this node
    std::vector<std::string> path_;

    // Whether to suppress empty list output.
    bool suppress_empty_list_;

    // Whether to preserve original proto field names
    bool preserve_proto_field_names_;

    // Whether to always print enums as ints
    bool use_ints_for_enums_;

    // Function for determining whether a field needs to be scrubbed or not.
    FieldScrubCallBack field_scrub_callback_;
  };

  // Creates a new Node and returns it. Caller owns memory of returned object.
  virtual Node* CreateNewNode(const std::string& name,
                              const google::protobuf::Type* type, NodeKind kind,
                              const DataPiece& data, bool is_placeholder,
                              const std::vector<std::string>& path,
                              bool suppress_empty_list,
                              bool preserve_proto_field_names,
                              bool use_ints_for_enums,
                              FieldScrubCallBack field_scrub_callback);

  // Creates a DataPiece containing the default value of the type of the field.
  static DataPiece CreateDefaultDataPieceForField(
      const google::protobuf::Field& field, const TypeInfo* typeinfo) {
    return CreateDefaultDataPieceForField(field, typeinfo, false);
  }

  // Same as the above but with a flag to use ints instead of enum names.
  static DataPiece CreateDefaultDataPieceForField(
      const google::protobuf::Field& field, const TypeInfo* typeinfo,
      bool use_ints_for_enums);

 protected:
  // Returns a pointer to current Node in tree.
  Node* current() { return current_; }

 private:
  // Populates children of "node" if it is an "any" Node and its real type has
  // been given.
  void MaybePopulateChildrenOfAny(Node* node);

  // Writes the root_ node to ow_ and resets the root_ and current_ pointer to
  // nullptr.
  void WriteRoot();

  // Adds or replaces the data_ of a primitive child node.
  void RenderDataPiece(absl::string_view name, const DataPiece& data);

  // Returns the default enum value as a DataPiece, or the first enum value if
  // there is no default. For proto3, where we cannot specify an explicit
  // default, a zero value will always be returned.
  static DataPiece FindEnumDefault(const google::protobuf::Field& field,
                                   const TypeInfo* typeinfo,
                                   bool use_ints_for_enums);

  // Type information for all the types used in the descriptor. Used to find
  // google::protobuf::Type of nested messages/enums.
  const TypeInfo* typeinfo_;
  // Whether the TypeInfo object is owned by this class.
  bool own_typeinfo_;
  // google::protobuf::Type of the root message type.
  const google::protobuf::Type& type_;
  // Holds copies of strings passed to RenderString.
  std::vector<std::unique_ptr<std::string>> string_values_;

  // The current Node. Owned by its parents.
  Node* current_;
  // The root Node.
  std::unique_ptr<Node> root_;
  // The stack to hold the path of Nodes from current_ to root_;
  std::stack<Node*> stack_;

  // Whether to suppress output of empty lists.
  bool suppress_empty_list_;

  // Whether to preserve original proto field names
  bool preserve_proto_field_names_;

  // Whether to always print enums as ints
  bool use_ints_for_enums_;

  // Function for determining whether a field needs to be scrubbed or not.
  FieldScrubCallBack field_scrub_callback_;

  ObjectWriter* ow_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_DEFAULT_VALUE_OBJECTWRITER_H_
