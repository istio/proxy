// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Type definitions for internal AST representation.
// CEL users should not directly depend on the definitions here.
#ifndef THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_EXPR_H_
#define THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_EXPR_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/constant.h"
#include "common/expr.h"

namespace cel::ast_internal {

// Temporary aliases that will be deleted in future.
using NullValue = std::nullptr_t;
using Bytes = cel::BytesConstant;
using Constant = cel::Constant;
using ConstantKind = cel::ConstantKind;
using Ident = cel::IdentExpr;
using Expr = cel::Expr;
using ExprKind = cel::ExprKind;
using Select = cel::SelectExpr;
using Call = cel::CallExpr;
using CreateList = cel::ListExpr;
using CreateStruct = cel::StructExpr;
using Comprehension = cel::ComprehensionExpr;

// An extension that was requested for the source expression.
class Extension {
 public:
  // Version
  class Version {
   public:
    Version() : major_(0), minor_(0) {}
    Version(int64_t major, int64_t minor) : major_(major), minor_(minor) {}

    Version(const Version& other) = default;
    Version(Version&& other) = default;
    Version& operator=(const Version& other) = default;
    Version& operator=(Version&& other) = default;

    static const Version& DefaultInstance();

    // Major version changes indicate different required support level from
    // the required components.
    int64_t major() const { return major_; }
    void set_major(int64_t val) { major_ = val; }

    // Minor version changes must not change the observed behavior from
    // existing implementations, but may be provided informationally.
    int64_t minor() const { return minor_; }
    void set_minor(int64_t val) { minor_ = val; }

    bool operator==(const Version& other) const {
      return major_ == other.major_ && minor_ == other.minor_;
    }

    bool operator!=(const Version& other) const { return !operator==(other); }

   private:
    int64_t major_;
    int64_t minor_;
  };

  // CEL component specifier.
  enum class Component {
    // Unspecified, default.
    kUnspecified,
    // Parser. Converts a CEL string to an AST.
    kParser,
    // Type checker. Checks that references in an AST are defined and types
    // agree.
    kTypeChecker,
    // Runtime. Evaluates a parsed and optionally checked CEL AST against a
    // context.
    kRuntime
  };

  static const Extension& DefaultInstance();

  Extension() = default;
  Extension(std::string id, std::unique_ptr<Version> version,
            std::vector<Component> affected_components)
      : id_(std::move(id)),
        affected_components_(std::move(affected_components)),
        version_(std::move(version)) {}

  Extension(const Extension& other);
  Extension(Extension&& other) = default;
  Extension& operator=(const Extension& other);
  Extension& operator=(Extension&& other) = default;

  // Identifier for the extension. Example: constant_folding
  const std::string& id() const { return id_; }
  void set_id(std::string id) { id_ = std::move(id); }

  // If set, the listed components must understand the extension for the
  // expression to evaluate correctly.
  //
  // This field has set semantics, repeated values should be deduplicated.
  const std::vector<Component>& affected_components() const {
    return affected_components_;
  }

  std::vector<Component>& mutable_affected_components() {
    return affected_components_;
  }

  // Version info. May be skipped if it isn't meaningful for the extension.
  // (for example constant_folding might always be v0.0).
  const Version& version() const {
    if (version_ == nullptr) {
      return Version::DefaultInstance();
    }
    return *version_;
  }

  Version& mutable_version() {
    if (version_ == nullptr) {
      version_ = std::make_unique<Version>();
    }
    return *version_;
  }

  void set_version(std::unique_ptr<Version> version) {
    version_ = std::move(version);
  }

  bool operator==(const Extension& other) const {
    return id_ == other.id_ &&
           affected_components_ == other.affected_components_ &&
           version() == other.version();
  }

  bool operator!=(const Extension& other) const { return !operator==(other); }

 private:
  std::string id_;
  std::vector<Component> affected_components_;
  std::unique_ptr<Version> version_;
};

// Source information collected at parse time.
class SourceInfo {
 public:
  SourceInfo() = default;
  SourceInfo(std::string syntax_version, std::string location,
             std::vector<int32_t> line_offsets,
             absl::flat_hash_map<int64_t, int32_t> positions,
             absl::flat_hash_map<int64_t, Expr> macro_calls,
             std::vector<Extension> extensions)
      : syntax_version_(std::move(syntax_version)),
        location_(std::move(location)),
        line_offsets_(std::move(line_offsets)),
        positions_(std::move(positions)),
        macro_calls_(std::move(macro_calls)),
        extensions_(std::move(extensions)) {}

  void set_syntax_version(std::string syntax_version) {
    syntax_version_ = std::move(syntax_version);
  }

  void set_location(std::string location) { location_ = std::move(location); }

  void set_line_offsets(std::vector<int32_t> line_offsets) {
    line_offsets_ = std::move(line_offsets);
  }

  void set_positions(absl::flat_hash_map<int64_t, int32_t> positions) {
    positions_ = std::move(positions);
  }

  void set_macro_calls(absl::flat_hash_map<int64_t, Expr> macro_calls) {
    macro_calls_ = std::move(macro_calls);
  }

  const std::string& syntax_version() const { return syntax_version_; }

  const std::string& location() const { return location_; }

  const std::vector<int32_t>& line_offsets() const { return line_offsets_; }

  std::vector<int32_t>& mutable_line_offsets() { return line_offsets_; }

  const absl::flat_hash_map<int64_t, int32_t>& positions() const {
    return positions_;
  }

  absl::flat_hash_map<int64_t, int32_t>& mutable_positions() {
    return positions_;
  }

  const absl::flat_hash_map<int64_t, Expr>& macro_calls() const {
    return macro_calls_;
  }

  absl::flat_hash_map<int64_t, Expr>& mutable_macro_calls() {
    return macro_calls_;
  }

  bool operator==(const SourceInfo& other) const {
    return syntax_version_ == other.syntax_version_ &&
           location_ == other.location_ &&
           line_offsets_ == other.line_offsets_ &&
           positions_ == other.positions_ &&
           macro_calls_ == other.macro_calls_ &&
           extensions_ == other.extensions_;
  }

  bool operator!=(const SourceInfo& other) const { return !operator==(other); }

  const std::vector<Extension>& extensions() const { return extensions_; }

  std::vector<Extension>& mutable_extensions() { return extensions_; }

 private:
  // The syntax version of the source, e.g. `cel1`.
  std::string syntax_version_;

  // The location name. All position information attached to an expression is
  // relative to this location.
  //
  // The location could be a file, UI element, or similar. For example,
  // `acme/app/AnvilPolicy.cel`.
  std::string location_;

  // Monotonically increasing list of code point offsets where newlines
  // `\n` appear.
  //
  // The line number of a given position is the index `i` where for a given
  // `id` the `line_offsets[i] < id_positions[id] < line_offsets[i+1]`. The
  // column may be derivd from `id_positions[id] - line_offsets[i]`.
  //
  // TODO(uncreated-issue/14): clarify this documentation
  std::vector<int32_t> line_offsets_;

  // A map from the parse node id (e.g. `Expr.id`) to the code point offset
  // within source.
  absl::flat_hash_map<int64_t, int32_t> positions_;

  // A map from the parse node id where a macro replacement was made to the
  // call `Expr` that resulted in a macro expansion.
  //
  // For example, `has(value.field)` is a function call that is replaced by a
  // `test_only` field selection in the AST. Likewise, the call
  // `list.exists(e, e > 10)` translates to a comprehension expression. The key
  // in the map corresponds to the expression id of the expanded macro, and the
  // value is the call `Expr` that was replaced.
  absl::flat_hash_map<int64_t, Expr> macro_calls_;

  // A list of tags for extensions that were used while parsing or type checking
  // the source expression. For example, optimizations that require special
  // runtime support may be specified.
  //
  // These are used to check feature support between components in separate
  // implementations. This can be used to either skip redundant work or
  // report an error if the extension is unsupported.
  std::vector<Extension> extensions_;
};

// CEL primitive types.
enum class PrimitiveType {
  // Unspecified type.
  kPrimitiveTypeUnspecified = 0,
  // Boolean type.
  kBool = 1,
  // Int64 type.
  //
  // Proto-based integer values are widened to int64.
  kInt64 = 2,
  // Uint64 type.
  //
  // Proto-based unsigned integer values are widened to uint64.
  kUint64 = 3,
  // Double type.
  //
  // Proto-based float values are widened to double values.
  kDouble = 4,
  // String type.
  kString = 5,
  // Bytes type.
  kBytes = 6,
};

// Well-known protobuf types treated with first-class support in CEL.
//
// TODO(uncreated-issue/15): represent well-known via abstract types (or however)
//   they will be named.
enum class WellKnownType {
  // Unspecified type.
  kWellKnownTypeUnspecified = 0,
  // Well-known protobuf.Any type.
  //
  // Any types are a polymorphic message type. During type-checking they are
  // treated like `DYN` types, but at runtime they are resolved to a specific
  // message type specified at evaluation time.
  kAny = 1,
  // Well-known protobuf.Timestamp type, internally referenced as `timestamp`.
  kTimestamp = 2,
  // Well-known protobuf.Duration type, internally referenced as `duration`.
  kDuration = 3,
};

class Type;

// List type with typed elements, e.g. `list<example.proto.MyMessage>`.
class ListType {
 public:
  ListType() = default;

  ListType(const ListType& rhs)
      : elem_type_(std::make_unique<Type>(rhs.elem_type())) {}
  ListType& operator=(const ListType& rhs) {
    elem_type_ = std::make_unique<Type>(rhs.elem_type());
    return *this;
  }
  ListType(ListType&& rhs) = default;
  ListType& operator=(ListType&& rhs) = default;

  explicit ListType(std::unique_ptr<Type> elem_type)
      : elem_type_(std::move(elem_type)) {}

  void set_elem_type(std::unique_ptr<Type> elem_type) {
    elem_type_ = std::move(elem_type);
  }

  bool has_elem_type() const { return elem_type_ != nullptr; }

  const Type& elem_type() const;

  Type& mutable_elem_type() {
    if (elem_type_ == nullptr) {
      elem_type_ = std::make_unique<Type>();
    }
    return *elem_type_;
  }

  bool operator==(const ListType& other) const;

 private:
  std::unique_ptr<Type> elem_type_;
};

// Map type with parameterized key and value types, e.g. `map<string, int>`.
class MapType {
 public:
  MapType() = default;
  MapType(std::unique_ptr<Type> key_type, std::unique_ptr<Type> value_type)
      : key_type_(std::move(key_type)), value_type_(std::move(value_type)) {}

  MapType(const MapType& rhs)
      : key_type_(std::make_unique<Type>(rhs.key_type())),
        value_type_(std::make_unique<Type>(rhs.value_type())) {}
  MapType& operator=(const MapType& rhs) {
    key_type_ = std::make_unique<Type>(rhs.key_type());
    value_type_ = std::make_unique<Type>(rhs.value_type());

    return *this;
  }
  MapType(MapType&& rhs) = default;
  MapType& operator=(MapType&& rhs) = default;

  void set_key_type(std::unique_ptr<Type> key_type) {
    key_type_ = std::move(key_type);
  }

  void set_value_type(std::unique_ptr<Type> value_type) {
    value_type_ = std::move(value_type);
  }

  bool has_key_type() const { return key_type_ != nullptr; }

  bool has_value_type() const { return value_type_ != nullptr; }

  const Type& key_type() const;

  const Type& value_type() const;

  bool operator==(const MapType& other) const;

  Type& mutable_key_type() {
    if (key_type_ == nullptr) {
      key_type_ = std::make_unique<Type>();
    }
    return *key_type_;
  }

  Type& mutable_value_type() {
    if (value_type_ == nullptr) {
      value_type_ = std::make_unique<Type>();
    }
    return *value_type_;
  }

 private:
  // The type of the key.
  std::unique_ptr<Type> key_type_;

  // The type of the value.
  std::unique_ptr<Type> value_type_;
};

// Function type with result and arg types.
//
// (--
// NOTE: function type represents a lambda-style argument to another function.
// Supported through macros, but not yet a first-class concept in CEL.
// --)
class FunctionType {
 public:
  FunctionType() = default;
  FunctionType(std::unique_ptr<Type> result_type, std::vector<Type> arg_types);

  FunctionType(const FunctionType& other);
  FunctionType& operator=(const FunctionType& other);
  FunctionType(FunctionType&&) = default;
  FunctionType& operator=(FunctionType&&) = default;

  void set_result_type(std::unique_ptr<Type> result_type) {
    result_type_ = std::move(result_type);
  }

  void set_arg_types(std::vector<Type> arg_types);

  bool has_result_type() const { return result_type_ != nullptr; }

  const Type& result_type() const;

  Type& mutable_result_type() {
    if (result_type_ == nullptr) {
      result_type_ = std::make_unique<Type>();
    }
    return *result_type_;
  }

  const std::vector<Type>& arg_types() const { return arg_types_; }

  std::vector<Type>& mutable_arg_types() { return arg_types_; }

  bool operator==(const FunctionType& other) const;

 private:
  // Result type of the function.
  std::unique_ptr<Type> result_type_;

  // Argument types of the function.
  std::vector<Type> arg_types_;
};

// Application defined abstract type.
//
// TODO(uncreated-issue/15): decide on final naming for this.
class AbstractType {
 public:
  AbstractType() = default;
  AbstractType(std::string name, std::vector<Type> parameter_types);

  void set_name(std::string name) { name_ = std::move(name); }

  void set_parameter_types(std::vector<Type> parameter_types);

  const std::string& name() const { return name_; }

  const std::vector<Type>& parameter_types() const { return parameter_types_; }

  std::vector<Type>& mutable_parameter_types() { return parameter_types_; }

  bool operator==(const AbstractType& other) const;

 private:
  // The fully qualified name of this abstract type.
  std::string name_;

  // Parameter types for this abstract type.
  std::vector<Type> parameter_types_;
};

// Wrapper of a primitive type, e.g. `google.protobuf.Int64Value`.
class PrimitiveTypeWrapper {
 public:
  explicit PrimitiveTypeWrapper(PrimitiveType type) : type_(std::move(type)) {}

  void set_type(PrimitiveType type) { type_ = std::move(type); }

  const PrimitiveType& type() const { return type_; }

  PrimitiveType& mutable_type() { return type_; }

  bool operator==(const PrimitiveTypeWrapper& other) const {
    return type_ == other.type_;
  }

 private:
  PrimitiveType type_;
};

// Protocol buffer message type.
//
// The `message_type` string specifies the qualified message type name. For
// example, `google.plus.Profile`.
class MessageType {
 public:
  MessageType() = default;
  explicit MessageType(std::string type) : type_(std::move(type)) {}

  void set_type(std::string type) { type_ = std::move(type); }

  const std::string& type() const { return type_; }

  bool operator==(const MessageType& other) const {
    return type_ == other.type_;
  }

 private:
  std::string type_;
};

// Type param type.
//
// The `type_param` string specifies the type parameter name, e.g. `list<E>`
// would be a `list_type` whose element type was a `type_param` type
// named `E`.
class ParamType {
 public:
  ParamType() = default;
  explicit ParamType(std::string type) : type_(std::move(type)) {}

  void set_type(std::string type) { type_ = std::move(type); }

  const std::string& type() const { return type_; }

  bool operator==(const ParamType& other) const { return type_ == other.type_; }

 private:
  std::string type_;
};

// Error type.
//
// During type-checking if an expression is an error, its type is propagated
// as the `ERROR` type. This permits the type-checker to discover other
// errors present in the expression.
enum class ErrorType { kErrorTypeValue = 0 };

struct UnspecifiedType : public absl::monostate {};

struct DynamicType : public absl::monostate {};

using TypeKind =
    absl::variant<UnspecifiedType, DynamicType, NullValue, PrimitiveType,
                  PrimitiveTypeWrapper, WellKnownType, ListType, MapType,
                  FunctionType, MessageType, ParamType,
                  absl_nullable std::unique_ptr<Type>, ErrorType, AbstractType>;

// Analogous to cel::expr::Type.
// Represents a CEL type.
//
// TODO(uncreated-issue/15): align with value.proto
class Type {
 public:
  Type() = default;
  explicit Type(TypeKind type_kind) : type_kind_(std::move(type_kind)) {}

  Type(const Type& other);
  Type& operator=(const Type& other);
  Type(Type&&) = default;
  Type& operator=(Type&&) = default;

  void set_type_kind(TypeKind type_kind) { type_kind_ = std::move(type_kind); }

  const TypeKind& type_kind() const { return type_kind_; }

  TypeKind& mutable_type_kind() { return type_kind_; }

  bool has_dyn() const {
    return absl::holds_alternative<DynamicType>(type_kind_);
  }

  bool has_null() const {
    return absl::holds_alternative<NullValue>(type_kind_);
  }

  bool has_primitive() const {
    return absl::holds_alternative<PrimitiveType>(type_kind_);
  }

  bool has_wrapper() const {
    return absl::holds_alternative<PrimitiveTypeWrapper>(type_kind_);
  }

  bool has_well_known() const {
    return absl::holds_alternative<WellKnownType>(type_kind_);
  }

  bool has_list_type() const {
    return absl::holds_alternative<ListType>(type_kind_);
  }

  bool has_map_type() const {
    return absl::holds_alternative<MapType>(type_kind_);
  }

  bool has_function() const {
    return absl::holds_alternative<FunctionType>(type_kind_);
  }

  bool has_message_type() const {
    return absl::holds_alternative<MessageType>(type_kind_);
  }

  bool has_type_param() const {
    return absl::holds_alternative<ParamType>(type_kind_);
  }

  bool has_type() const {
    return absl::holds_alternative<std::unique_ptr<Type>>(type_kind_);
  }

  bool has_error() const {
    return absl::holds_alternative<ErrorType>(type_kind_);
  }

  bool has_abstract_type() const {
    return absl::holds_alternative<AbstractType>(type_kind_);
  }

  NullValue null() const {
    auto* value = absl::get_if<NullValue>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return nullptr;
  }

  PrimitiveType primitive() const {
    auto* value = absl::get_if<PrimitiveType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return PrimitiveType::kPrimitiveTypeUnspecified;
  }

  PrimitiveType wrapper() const {
    auto* value = absl::get_if<PrimitiveTypeWrapper>(&type_kind_);
    if (value != nullptr) {
      return value->type();
    }
    return PrimitiveType::kPrimitiveTypeUnspecified;
  }

  WellKnownType well_known() const {
    auto* value = absl::get_if<WellKnownType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return WellKnownType::kWellKnownTypeUnspecified;
  }

  const ListType& list_type() const {
    auto* value = absl::get_if<ListType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const ListType* default_list_type = new ListType();
    return *default_list_type;
  }

  const MapType& map_type() const {
    auto* value = absl::get_if<MapType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const MapType* default_map_type = new MapType();
    return *default_map_type;
  }

  const FunctionType& function() const {
    auto* value = absl::get_if<FunctionType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const FunctionType* default_function_type = new FunctionType();
    return *default_function_type;
  }

  const MessageType& message_type() const {
    auto* value = absl::get_if<MessageType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const MessageType* default_message_type = new MessageType();
    return *default_message_type;
  }

  const ParamType& type_param() const {
    auto* value = absl::get_if<ParamType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const ParamType* default_param_type = new ParamType();
    return *default_param_type;
  }

  const Type& type() const;

  ErrorType error_type() const {
    auto* value = absl::get_if<ErrorType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return ErrorType::kErrorTypeValue;
  }

  const AbstractType& abstract_type() const {
    auto* value = absl::get_if<AbstractType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const AbstractType* default_abstract_type = new AbstractType();
    return *default_abstract_type;
  }

  bool operator==(const Type& other) const {
    if (absl::holds_alternative<std::unique_ptr<Type>>(type_kind_) &&
        absl::holds_alternative<std::unique_ptr<Type>>(other.type_kind_)) {
      const auto& self_type = absl::get<std::unique_ptr<Type>>(type_kind_);
      const auto& other_type =
          absl::get<std::unique_ptr<Type>>(other.type_kind_);
      if (self_type == nullptr || other_type == nullptr) {
        return self_type == other_type;
      }
      return *self_type == *other_type;
    }
    return type_kind_ == other.type_kind_;
  }

 private:
  TypeKind type_kind_;
};

// Describes a resolved reference to a declaration.
class Reference {
 public:
  Reference() = default;

  Reference(std::string name, std::vector<std::string> overload_id,
            Constant value)
      : name_(std::move(name)),
        overload_id_(std::move(overload_id)),
        value_(std::move(value)) {}

  void set_name(std::string name) { name_ = std::move(name); }

  void set_overload_id(std::vector<std::string> overload_id) {
    overload_id_ = std::move(overload_id);
  }

  void set_value(Constant value) { value_ = std::move(value); }

  const std::string& name() const { return name_; }

  const std::vector<std::string>& overload_id() const { return overload_id_; }

  const Constant& value() const {
    if (value_.has_value()) {
      return value_.value();
    }
    static const Constant* default_constant = new Constant;
    return *default_constant;
  }

  std::vector<std::string>& mutable_overload_id() { return overload_id_; }

  Constant& mutable_value() {
    if (!value_.has_value()) {
      value_.emplace();
    }
    return *value_;
  }

  bool has_value() const { return value_.has_value(); }

  bool operator==(const Reference& other) const {
    return name_ == other.name_ && overload_id_ == other.overload_id_ &&
           value() == other.value();
  }

 private:
  // The fully qualified name of the declaration.
  std::string name_;
  // For references to functions, this is a list of `Overload.overload_id`
  // values which match according to typing rules.
  //
  // If the list has more than one element, overload resolution among the
  // presented candidates must happen at runtime because of dynamic types. The
  // type checker attempts to narrow down this list as much as possible.
  //
  // Empty if this is not a reference to a [Decl.FunctionDecl][].
  std::vector<std::string> overload_id_;
  // For references to constants, this may contain the value of the
  // constant if known at compile time.
  absl::optional<Constant> value_;
};

////////////////////////////////////////////////////////////////////////
// Implementation details
////////////////////////////////////////////////////////////////////////

inline FunctionType::FunctionType(std::unique_ptr<Type> result_type,
                                  std::vector<Type> arg_types)
    : result_type_(std::move(result_type)), arg_types_(std::move(arg_types)) {}

inline void FunctionType::set_arg_types(std::vector<Type> arg_types) {
  arg_types_ = std::move(arg_types);
}

inline AbstractType::AbstractType(std::string name,
                                  std::vector<Type> parameter_types)
    : name_(std::move(name)), parameter_types_(std::move(parameter_types)) {}

inline void AbstractType::set_parameter_types(
    std::vector<Type> parameter_types) {
  parameter_types_ = std::move(parameter_types);
}

inline bool AbstractType::operator==(const AbstractType& other) const {
  return name_ == other.name_ && parameter_types_ == other.parameter_types_;
}

}  // namespace cel::ast_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_EXPR_H_
