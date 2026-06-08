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
// Type definitions for auxiliary structures in the AST.
//
// These are more direct equivalents to the public protobuf definitions.
//
// IWYU pragma: private, include "common/ast.h"
#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_METADATA_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_METADATA_H_

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

namespace cel {

// An extension that was requested for the source expression.
class ExtensionSpec {
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

  static const ExtensionSpec& DefaultInstance();

  ExtensionSpec() = default;
  ExtensionSpec(std::string id, std::unique_ptr<Version> version,
                std::vector<Component> affected_components)
      : id_(std::move(id)),
        affected_components_(std::move(affected_components)),
        version_(std::move(version)) {}

  ExtensionSpec(const ExtensionSpec& other);
  ExtensionSpec(ExtensionSpec&& other) = default;
  ExtensionSpec& operator=(const ExtensionSpec& other);
  ExtensionSpec& operator=(ExtensionSpec&& other) = default;

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

  bool operator==(const ExtensionSpec& other) const {
    return id_ == other.id_ &&
           affected_components_ == other.affected_components_ &&
           version() == other.version();
  }

  bool operator!=(const ExtensionSpec& other) const {
    return !operator==(other);
  }

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
             std::vector<ExtensionSpec> extensions)
      : syntax_version_(std::move(syntax_version)),
        location_(std::move(location)),
        line_offsets_(std::move(line_offsets)),
        positions_(std::move(positions)),
        macro_calls_(std::move(macro_calls)),
        extensions_(std::move(extensions)) {}

  SourceInfo(const SourceInfo& other) = default;
  SourceInfo(SourceInfo&& other) = default;
  SourceInfo& operator=(const SourceInfo& other) = default;
  SourceInfo& operator=(SourceInfo&& other) = default;

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

  const std::vector<ExtensionSpec>& extensions() const { return extensions_; }

  std::vector<ExtensionSpec>& mutable_extensions() { return extensions_; }

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
  std::vector<ExtensionSpec> extensions_;
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
enum class WellKnownTypeSpec {
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

// forward declare for recursive types.
class TypeSpec;

// List type with typed elements, e.g. `list<example.proto.MyMessage>`.
class ListTypeSpec {
 public:
  ListTypeSpec() = default;

  ListTypeSpec(const ListTypeSpec& rhs);
  ListTypeSpec& operator=(const ListTypeSpec& rhs);
  ListTypeSpec(ListTypeSpec&& rhs) = default;
  ListTypeSpec& operator=(ListTypeSpec&& rhs) = default;

  explicit ListTypeSpec(std::unique_ptr<TypeSpec> elem_type);

  void set_elem_type(std::unique_ptr<TypeSpec> elem_type);

  bool has_elem_type() const { return elem_type_ != nullptr; }

  const TypeSpec& elem_type() const;

  TypeSpec& mutable_elem_type();

  bool operator==(const ListTypeSpec& other) const;

 private:
  std::unique_ptr<TypeSpec> elem_type_;
};

// Map type specifier with parameterized key and value types, e.g.
// `map<string, int>`.
class MapTypeSpec {
 public:
  MapTypeSpec() = default;
  MapTypeSpec(std::unique_ptr<TypeSpec> key_type,
              std::unique_ptr<TypeSpec> value_type);

  MapTypeSpec(const MapTypeSpec& rhs);
  MapTypeSpec& operator=(const MapTypeSpec& rhs);
  MapTypeSpec(MapTypeSpec&& rhs) = default;
  MapTypeSpec& operator=(MapTypeSpec&& rhs) = default;

  void set_key_type(std::unique_ptr<TypeSpec> key_type);

  void set_value_type(std::unique_ptr<TypeSpec> value_type);

  bool has_key_type() const { return key_type_ != nullptr; }

  bool has_value_type() const { return value_type_ != nullptr; }

  const TypeSpec& key_type() const;

  const TypeSpec& value_type() const;

  bool operator==(const MapTypeSpec& other) const;

  TypeSpec& mutable_key_type();

  TypeSpec& mutable_value_type();

 private:
  // The type of the key.
  std::unique_ptr<TypeSpec> key_type_;

  // The type of the value.
  std::unique_ptr<TypeSpec> value_type_;
};

// Function type specifiers with result and arg types.
//
// NOTE: function type represents a lambda-style argument to another function.
// Supported through macros, but not yet a first-class concept in CEL.
class FunctionTypeSpec {
 public:
  FunctionTypeSpec() = default;
  FunctionTypeSpec(std::unique_ptr<TypeSpec> result_type,
                   std::vector<TypeSpec> arg_types);

  FunctionTypeSpec(const FunctionTypeSpec& other);
  FunctionTypeSpec& operator=(const FunctionTypeSpec& other);
  FunctionTypeSpec(FunctionTypeSpec&&) = default;
  FunctionTypeSpec& operator=(FunctionTypeSpec&&) = default;

  void set_result_type(std::unique_ptr<TypeSpec> result_type);

  void set_arg_types(std::vector<TypeSpec> arg_types);

  bool has_result_type() const { return result_type_ != nullptr; }

  const TypeSpec& result_type() const;

  TypeSpec& mutable_result_type();

  const std::vector<TypeSpec>& arg_types() const { return arg_types_; }

  std::vector<TypeSpec>& mutable_arg_types() { return arg_types_; }

  bool operator==(const FunctionTypeSpec& other) const;

 private:
  // Result type of the function.
  std::unique_ptr<TypeSpec> result_type_;

  // Argument types of the function.
  std::vector<TypeSpec> arg_types_;
};

// Application defined abstract type.
//
// Abstract types provide a name as an identifier for the application, and
// optionally one or more type parameters.
//
// For cel::Type representation, see OpaqueType.
class AbstractType {
 public:
  AbstractType() = default;
  AbstractType(std::string name, std::vector<TypeSpec> parameter_types);

  void set_name(std::string name) { name_ = std::move(name); }

  void set_parameter_types(std::vector<TypeSpec> parameter_types);

  const std::string& name() const { return name_; }

  const std::vector<TypeSpec>& parameter_types() const {
    return parameter_types_;
  }

  std::vector<TypeSpec>& mutable_parameter_types() { return parameter_types_; }

  bool operator==(const AbstractType& other) const;

 private:
  // The fully qualified name of this abstract type.
  std::string name_;

  // Parameter types for this abstract type.
  std::vector<TypeSpec> parameter_types_;
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

// Protocol buffer message type specifier.
//
// The `message_type` string specifies the qualified message type name. For
// example, `google.plus.Profile`. This must be mapped to a google::protobuf::Descriptor
// for type checking.
class MessageTypeSpec {
 public:
  MessageTypeSpec() = default;
  explicit MessageTypeSpec(std::string type) : type_(std::move(type)) {}

  void set_type(std::string type) { type_ = std::move(type); }

  const std::string& type() const { return type_; }

  bool operator==(const MessageTypeSpec& other) const {
    return type_ == other.type_;
  }

 private:
  std::string type_;
};

// TypeSpec param type.
//
// The `type_param` string specifies the type parameter name, e.g. `list<E>`
// would be a `list_type` whose element type was a `type_param` type
// named `E`.
class ParamTypeSpec {
 public:
  ParamTypeSpec() = default;
  explicit ParamTypeSpec(std::string type) : type_(std::move(type)) {}

  void set_type(std::string type) { type_ = std::move(type); }

  const std::string& type() const { return type_; }

  bool operator==(const ParamTypeSpec& other) const {
    return type_ == other.type_;
  }

 private:
  std::string type_;
};

// Error type specifier.
//
// During type-checking if an expression is an error, its type is propagated
// as the `ERROR` type. This permits the type-checker to discover other
// errors present in the expression.
enum class ErrorTypeSpec { kValue = 0 };

using UnsetTypeSpec = absl::monostate;

struct DynTypeSpec {};

inline bool operator==(const DynTypeSpec&, const DynTypeSpec&) { return true; }
inline bool operator!=(const DynTypeSpec&, const DynTypeSpec&) { return false; }

struct NullTypeSpec {};
inline bool operator==(const NullTypeSpec&, const NullTypeSpec&) {
  return true;
}
inline bool operator!=(const NullTypeSpec&, const NullTypeSpec&) {
  return false;
}

using TypeSpecKind =
    absl::variant<UnsetTypeSpec, DynTypeSpec, NullTypeSpec, PrimitiveType,
                  PrimitiveTypeWrapper, WellKnownTypeSpec, ListTypeSpec,
                  MapTypeSpec, FunctionTypeSpec, MessageTypeSpec, ParamTypeSpec,
                  absl_nullable std::unique_ptr<TypeSpec>, ErrorTypeSpec,
                  AbstractType>;

// Analogous to cel::expr::Type.
// Represents a CEL type.
//
// TODO(uncreated-issue/15): align with value.proto
class TypeSpec {
 public:
  TypeSpec() = default;
  explicit TypeSpec(TypeSpecKind type_kind)
      : type_kind_(std::move(type_kind)) {}

  TypeSpec(const TypeSpec& other);
  TypeSpec& operator=(const TypeSpec& other);
  TypeSpec(TypeSpec&&) = default;
  TypeSpec& operator=(TypeSpec&&) = default;

  void set_type_kind(TypeSpecKind type_kind) {
    type_kind_ = std::move(type_kind);
  }

  const TypeSpecKind& type_kind() const { return type_kind_; }

  TypeSpecKind& mutable_type_kind() { return type_kind_; }

  bool has_dyn() const {
    return absl::holds_alternative<DynTypeSpec>(type_kind_);
  }

  bool has_null() const {
    return absl::holds_alternative<NullTypeSpec>(type_kind_);
  }

  bool has_primitive() const {
    return absl::holds_alternative<PrimitiveType>(type_kind_);
  }

  bool has_wrapper() const {
    return absl::holds_alternative<PrimitiveTypeWrapper>(type_kind_);
  }

  bool has_well_known() const {
    return absl::holds_alternative<WellKnownTypeSpec>(type_kind_);
  }

  bool has_list_type() const {
    return absl::holds_alternative<ListTypeSpec>(type_kind_);
  }

  bool has_map_type() const {
    return absl::holds_alternative<MapTypeSpec>(type_kind_);
  }

  bool has_function() const {
    return absl::holds_alternative<FunctionTypeSpec>(type_kind_);
  }

  bool has_message_type() const {
    return absl::holds_alternative<MessageTypeSpec>(type_kind_);
  }

  bool has_type_param() const {
    return absl::holds_alternative<ParamTypeSpec>(type_kind_);
  }

  bool has_type() const {
    return absl::holds_alternative<std::unique_ptr<TypeSpec>>(type_kind_);
  }

  bool has_error() const {
    return absl::holds_alternative<ErrorTypeSpec>(type_kind_);
  }

  bool has_abstract_type() const {
    return absl::holds_alternative<AbstractType>(type_kind_);
  }

  NullTypeSpec null() const {
    auto* value = absl::get_if<NullTypeSpec>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return {};
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

  WellKnownTypeSpec well_known() const {
    auto* value = absl::get_if<WellKnownTypeSpec>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return WellKnownTypeSpec::kWellKnownTypeUnspecified;
  }

  const ListTypeSpec& list_type() const {
    auto* value = absl::get_if<ListTypeSpec>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const ListTypeSpec* default_list_type = new ListTypeSpec();
    return *default_list_type;
  }

  const MapTypeSpec& map_type() const {
    auto* value = absl::get_if<MapTypeSpec>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const MapTypeSpec* default_map_type = new MapTypeSpec();
    return *default_map_type;
  }

  const FunctionTypeSpec& function() const {
    auto* value = absl::get_if<FunctionTypeSpec>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const FunctionTypeSpec* default_function_type =
        new FunctionTypeSpec();
    return *default_function_type;
  }

  const MessageTypeSpec& message_type() const {
    auto* value = absl::get_if<MessageTypeSpec>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const MessageTypeSpec* default_message_type = new MessageTypeSpec();
    return *default_message_type;
  }

  const ParamTypeSpec& type_param() const {
    auto* value = absl::get_if<ParamTypeSpec>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const ParamTypeSpec* default_param_type = new ParamTypeSpec();
    return *default_param_type;
  }

  const TypeSpec& type() const;

  ErrorTypeSpec error_type() const {
    auto* value = absl::get_if<ErrorTypeSpec>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return ErrorTypeSpec::kValue;
  }

  const AbstractType& abstract_type() const {
    auto* value = absl::get_if<AbstractType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const AbstractType* default_abstract_type = new AbstractType();
    return *default_abstract_type;
  }

  bool operator==(const TypeSpec& other) const {
    if (absl::holds_alternative<std::unique_ptr<TypeSpec>>(type_kind_) &&
        absl::holds_alternative<std::unique_ptr<TypeSpec>>(other.type_kind_)) {
      const auto& self_type = absl::get<std::unique_ptr<TypeSpec>>(type_kind_);
      const auto& other_type =
          absl::get<std::unique_ptr<TypeSpec>>(other.type_kind_);
      if (self_type == nullptr || other_type == nullptr) {
        return self_type == other_type;
      }
      return *self_type == *other_type;
    }
    return type_kind_ == other.type_kind_;
  }

 private:
  TypeSpecKind type_kind_;
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

  Reference(const Reference& other) = default;
  Reference& operator=(const Reference& other) = default;
  Reference(Reference&&) = default;
  Reference& operator=(Reference&&) = default;

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
// Out-of-line method declarations
////////////////////////////////////////////////////////////////////////

inline ListTypeSpec::ListTypeSpec(const ListTypeSpec& rhs)
    : elem_type_(std::make_unique<TypeSpec>(rhs.elem_type())) {}

inline ListTypeSpec& ListTypeSpec::operator=(const ListTypeSpec& rhs) {
  elem_type_ = std::make_unique<TypeSpec>(rhs.elem_type());
  return *this;
}

inline ListTypeSpec::ListTypeSpec(std::unique_ptr<TypeSpec> elem_type)
    : elem_type_(std::move(elem_type)) {}

inline void ListTypeSpec::set_elem_type(std::unique_ptr<TypeSpec> elem_type) {
  elem_type_ = std::move(elem_type);
}

inline TypeSpec& ListTypeSpec::mutable_elem_type() {
  if (elem_type_ == nullptr) {
    elem_type_ = std::make_unique<TypeSpec>();
  }
  return *elem_type_;
}

inline MapTypeSpec::MapTypeSpec(std::unique_ptr<TypeSpec> key_type,
                                std::unique_ptr<TypeSpec> value_type)
    : key_type_(std::move(key_type)), value_type_(std::move(value_type)) {}

inline MapTypeSpec::MapTypeSpec(const MapTypeSpec& rhs)
    : key_type_(std::make_unique<TypeSpec>(rhs.key_type())),
      value_type_(std::make_unique<TypeSpec>(rhs.value_type())) {}

inline MapTypeSpec& MapTypeSpec::operator=(const MapTypeSpec& rhs) {
  key_type_ = std::make_unique<TypeSpec>(rhs.key_type());
  value_type_ = std::make_unique<TypeSpec>(rhs.value_type());
  return *this;
}

inline void MapTypeSpec::set_key_type(std::unique_ptr<TypeSpec> key_type) {
  key_type_ = std::move(key_type);
}

inline void MapTypeSpec::set_value_type(std::unique_ptr<TypeSpec> value_type) {
  value_type_ = std::move(value_type);
}

inline TypeSpec& MapTypeSpec::mutable_key_type() {
  if (key_type_ == nullptr) {
    key_type_ = std::make_unique<TypeSpec>();
  }
  return *key_type_;
}

inline TypeSpec& MapTypeSpec::mutable_value_type() {
  if (value_type_ == nullptr) {
    value_type_ = std::make_unique<TypeSpec>();
  }
  return *value_type_;
}

inline void FunctionTypeSpec::set_result_type(
    std::unique_ptr<TypeSpec> result_type) {
  result_type_ = std::move(result_type);
}

inline TypeSpec& FunctionTypeSpec::mutable_result_type() {
  if (result_type_ == nullptr) {
    result_type_ = std::make_unique<TypeSpec>();
  }
  return *result_type_;
}

////////////////////////////////////////////////////////////////////////
// Implementation details
////////////////////////////////////////////////////////////////////////

inline FunctionTypeSpec::FunctionTypeSpec(std::unique_ptr<TypeSpec> result_type,
                                          std::vector<TypeSpec> arg_types)
    : result_type_(std::move(result_type)), arg_types_(std::move(arg_types)) {}

inline void FunctionTypeSpec::set_arg_types(std::vector<TypeSpec> arg_types) {
  arg_types_ = std::move(arg_types);
}

inline AbstractType::AbstractType(std::string name,
                                  std::vector<TypeSpec> parameter_types)
    : name_(std::move(name)), parameter_types_(std::move(parameter_types)) {}

inline void AbstractType::set_parameter_types(
    std::vector<TypeSpec> parameter_types) {
  parameter_types_ = std::move(parameter_types);
}

inline bool AbstractType::operator==(const AbstractType& other) const {
  return name_ == other.name_ && parameter_types_ == other.parameter_types_;
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_METADATA_H_
