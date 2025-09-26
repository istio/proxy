#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/common.h"
#include "common/macros.h"

namespace Hessian2 {

constexpr const char* UntypedMapMagicString = "untypedmap";
constexpr const char* UntypedListMagicString = "untypedlist";
constexpr const char* NullMagicString = "null";

template <class T>
class OptRef : public absl::optional<std::reference_wrapper<T>> {
 public:
  // Inherit absl::optional's constructors.
  using absl::optional<std::reference_wrapper<T>>::optional;

  // Rewrite operator-> then we can use the OptRef as a pointer and not need
  // to use the value().get() to get the inner object. Please ensure that the
  // OptRef has_value() will return true before using operator->.
  T* operator->() { return &(this->value().get()); }
  const T* operator->() const { return &(this->value().get()); }

  // To get the pointer of the inner object. If the OptRef has_value() return
  // false then this method will return nullptr.
  T* ptr() { return this->has_value() ? &(this->value().get()) : nullptr; }
  const T* ptr() const {
    return this->has_value() ? &(this->value().get()) : nullptr;
  }

  // To get the reference of the inner object. Please ensure that the OptRef
  // has_value() will return true before using these methods.
  T& ref() { return this->value().get(); }
  const T& ref() const { return this->value().get(); }
};

template <class T>
using OptConstRef = OptRef<const T>;

using Binary = std::vector<uint8_t>;

class Object;
using ObjectPtr = std::unique_ptr<Object>;

#define EmptyToMethod(Name, Type)                                      \
  virtual OptConstRef<Type> to##Name() const { return absl::nullopt; } \
  virtual OptRef<Type> toMutable##Name() { return absl::nullopt; }

#define ValidToMethod(Name, Type)                                          \
  OptConstRef<Type> to##Name() const override { return std::cref(data_); } \
  OptRef<Type> toMutable##Name() override { return std::ref(data_); }

class Object {
 public:
  struct ObjectHasher {
    using is_transparent = void;

    std::size_t operator()(const ObjectPtr& k) const { return k->hash(); }
    // In most of cases, string is used as map key. And by this function, we can
    // search map entry by the string view directly and avoid creation of
    // StringObject.
    std::size_t operator()(absl::string_view k) const {
      return absl::Hash<absl::string_view>{}(k);
    }
  };

  struct ObjectEqual {
    using is_transparent = void;

    bool operator()(const ObjectPtr& left, const ObjectPtr& right) const {
      return left->equal(*right);
    }
    // In most of cases, string is used as map key. And by this function, we can
    // search map entry by the string view directly and avoid creation of
    // StringObject.
    bool operator()(const ObjectPtr& left, absl::string_view right) const {
      if (left->type() != Type::String) {
        return false;
      }
      return absl::string_view(left->toString().value().get()) == right;
    }
    bool operator()(absl::string_view left, const ObjectPtr& right) const {
      if (right->type() != Type::String) {
        return false;
      }
      return absl::string_view(right->toString().value().get()) == left;
    }
  };

  struct TypeRef {
    bool operator==(const TypeRef& other) const { return other.type_ == type_; }
    TypeRef(absl::string_view type) : type_(type) {}
    std::string type_;
  };

  struct RawDefinition {
    bool operator==(const RawDefinition& other) const {
      return other.type_ == type_ && other.field_names_ == field_names_;
    }

    bool operator!=(const RawDefinition& other) const {
      return !(other == *this);
    }
    RawDefinition() = default;
    RawDefinition(const std::string& type, std::vector<std::string>&& field)
        : type_(type), field_names_(std::move(field)) {}
    RawDefinition(const RawDefinition& def) = default;
    std::string toDebugString() const {
      std::ostringstream ostream;
      for (auto& o : field_names_) {
        ostream << o << " ";
      }
      return absl::StrFormat("type: %s , field_list: %s", type_, ostream.str());
    }
    size_t hash() const {
      size_t hash = 0;
      hash = std::hash<std::string>{}(type_);
      Utils::hashCombine(hash, field_names_.size());
      return hash;
    }

    std::string type_;
    std::vector<std::string> field_names_;
  };

  using RawDefinitionSharedPtr = std::shared_ptr<RawDefinition>;

  struct Definition {
    Definition() = default;
    Definition(const RawDefinitionSharedPtr data) : data_(std::move(data)) {}
    bool operator==(const Definition& other) const {
      return *other.data_ == *data_;
    }
    bool operator!=(const Definition& other) const { return !(other == *this); }

    RawDefinitionSharedPtr data_;
  };

  // TODO(tianqian.zyf): Check that the definition and values size are
  // consistent
  struct ClassInstance {
    bool operator==(const ClassInstance& other) const {
      if (*def_ != *other.def_) {
        return false;
      }
      if (data_.size() != other.data_.size()) {
        return false;
      }

      for (size_t i = 0; i < data_.size(); i++) {
        if (!data_[i]->equal(*other.data_[i])) {
          return false;
        }
      }

      return true;
    }
    size_t hash() const {
      ABSL_ASSERT(def_);
      size_t hash = def_->hash();
      Utils::hashCombine(hash, data_.size());
      return hash;
    }
    RawDefinitionSharedPtr def_;
    std::vector<ObjectPtr> data_;
  };

  using UntypedMap =
      absl::node_hash_map<ObjectPtr, ObjectPtr, ObjectHasher, ObjectEqual>;

  struct TypedMap {
    TypedMap() = default;
    TypedMap(std::string&& type_name, UntypedMap&& values)
        : type_name_(std::move(type_name)),
          field_name_and_value_(std::move(values)) {}

    bool operator==(const TypedMap& other) const {
      if (other.type_name_ != type_name_) {
        return false;
      }

      if (other.field_name_and_value_.size() !=
          other.field_name_and_value_.size()) {
        return false;
      }

      for (const auto& elem : field_name_and_value_) {
        auto it = other.field_name_and_value_.find(elem.first);
        if (it == other.field_name_and_value_.end()) {
          return false;
        }

        if (*(it->second) != *(elem.second)) {
          return false;
        }
      }
      return true;
    }

    std::string type_name_;
    UntypedMap field_name_and_value_;
  };

  using UntypedList = std::vector<ObjectPtr>;

  struct TypedList {
    TypedList() = default;
    TypedList(std::string&& type_name, UntypedList&& values)
        : type_name_(std::move(type_name)), values_(std::move(values)) {}
    bool operator==(const TypedList& other) const {
      if (other.type_name_ != type_name_) {
        return false;
      }

      if (values_.size() != other.values_.size()) {
        return false;
      }

      for (size_t i = 0; i < values_.size(); i++) {
        if (*(values_[i]) != *(other.values_[i])) {
          return false;
        }
      }

      return true;
    }
    std::string type_name_;
    UntypedList values_;
  };

  enum class Type : unsigned char {
    Binary = 0,
    Boolean,
    Date,
    Double,
    Integer,
    Long,
    Null,
    Ref,
    String,
    TypedList,
    UntypedList,
    TypedMap,
    UntypedMap,
    Class,
  };

  Object() = default;
  virtual ~Object() = default;

  template <typename T>
  T& asType() {
    assert(dynamic_cast<T*>(this) != nullptr);
    return *static_cast<T*>(this);
  }

  EmptyToMethod(Boolean, bool);
  EmptyToMethod(Integer, int32_t);
  EmptyToMethod(Long, int64_t);
  EmptyToMethod(Double, double);
  EmptyToMethod(Date, std::chrono::milliseconds);
  EmptyToMethod(Binary, Binary);
  EmptyToMethod(String, std::string);
  EmptyToMethod(TypedList, TypedList);
  EmptyToMethod(UntypedList, UntypedList);
  EmptyToMethod(TypedMap, TypedMap);
  EmptyToMethod(UntypedMap, UntypedMap);
  EmptyToMethod(ClassInstance, ClassInstance);

  virtual absl::optional<Object*> toRefDest() const { return absl::nullopt; }

  virtual bool equal(const Object& o) const = 0;
  virtual Type type() const = 0;
  // Used to provide a hash value for a hash map, but the hash value returned by
  // this hash method cannot be used as a unique identifier because its
  // implementation has a high probability of colliding.
  virtual size_t hash() const = 0;

  virtual std::string toDebugString() const {
    return absl::StrFormat("Type enum value: %d", static_cast<uint8_t>(type()));
  }

  bool operator==(const Object& other) const { return equal(other); }
  bool operator!=(const Object& other) const { return !equal(other); }
};

#define GENERATE_STD_HASH(DataType) \
  size_t hash() const override { return std::hash<DataType>{}(data_); }

#define DO_NOT_GENERATE_HASH(DataType)

#define GENERATE_TRIVIAL_METHOD(ObjectType, DataType, MethodName, \
                                GENERATE_HASH)                    \
  ValidToMethod(MethodName, DataType);                            \
  Type type() const override { return ObjectType; }               \
  bool equal(const Object& o) const override {                    \
    if (o.type() != ObjectType) {                                 \
      return false;                                               \
    }                                                             \
    ABSL_ASSERT(o.to##MethodName().has_value());                  \
    return o.to##MethodName().value().get() == data_;             \
  }                                                               \
  GENERATE_HASH(DataType);

class NullObject : public Object {
 public:
  Type type() const override { return Type::Null; }

  // Object
  size_t hash() const override {
    // TODO(tianqian.zyf): Provide a unique hash value.
    // GCC does not support std::hash<std::nullptr_t>{}(nullptr),
    // so only set a special hash value for NullObject.
    return std::hash<std::string>{}(NullMagicString);
  }
  bool equal(const Object& o) const override { return o.type() == Type::Null; }
  std::string toDebugString() const override {
    return std::string("Type: Null");
  }
};

class RefObject : public Object {
 public:
  // We need to make sure that the object that the RefObject points to has a
  // longer lifetime than the RefObject
  RefObject(Object* data) : data_(data) {}

  // Object.
  Type type() const override { return Type::Ref; }
  absl::optional<Object*> toRefDest() const override { return data_; }
  bool equal(const Object& o) const override {
    return o.type() == Type::Ref && o.toRefDest() == data_;
  }
  size_t hash() const override { return data_->hash(); }
  std::string toDebugString() const override {
    return absl::StrFormat("Type: Ref, target address: %p, value[%s]", data_,
                           data_->toDebugString());
  }

 private:
  Object* data_;
};

class BooleanObject : public Object {
 public:
  BooleanObject(bool data) : data_(data) {}

  // Object
  GENERATE_TRIVIAL_METHOD(Type::Boolean, bool, Boolean, GENERATE_STD_HASH);
  std::string toDebugString() const override {
    return absl::StrFormat("Type: boolean, value[%s]",
                           data_ ? "true" : "false");
  }

 private:
  bool data_;
};

class IntegerObject : public Object {
 public:
  IntegerObject(int32_t data) : data_(data) {}

  // Object
  GENERATE_TRIVIAL_METHOD(Type::Integer, int32_t, Integer, GENERATE_STD_HASH);
  std::string toDebugString() const override {
    return absl::StrFormat("Type: integer, value[%d]", data_);
  }

 private:
  int32_t data_;
};

class DoubleObject : public Object {
 public:
  DoubleObject(double data) : data_(data) {}

  // Object
  GENERATE_TRIVIAL_METHOD(Type::Double, double, Double, GENERATE_STD_HASH);
  std::string toDebugString() const override {
    return absl::StrFormat("Type: double, value[%f]", data_);
  }

 private:
  double data_;
};

class DateObject : public Object {
 public:
  DateObject(std::chrono::milliseconds data) : data_(data) {}

  // Object
  GENERATE_TRIVIAL_METHOD(Type::Date, std::chrono::milliseconds, Date,
                          DO_NOT_GENERATE_HASH)
  size_t hash() const override { return std::hash<size_t>{}(data_.count()); }
  std::string toDebugString() const override {
    return absl::StrFormat("Type: double, value[%d ms]", data_.count());
  }

 private:
  std::chrono::milliseconds data_;
};

class LongObject : public Object {
 public:
  LongObject(int64_t data) : data_(data) {}

  // Object
  GENERATE_TRIVIAL_METHOD(Type::Long, int64_t, Long, GENERATE_STD_HASH);
  std::string toDebugString() const override {
    return absl::StrFormat("Type: long, value[%d]", data_);
  }

 private:
  int64_t data_;
};

class BinaryObject : public Object {
 public:
  BinaryObject(Binary&& data) : data_(std::move(data)) {}
  BinaryObject(const Binary& data) : data_(data) {}
  BinaryObject(std::unique_ptr<Binary>&& data) : data_(std::move(*data)) {}
  virtual ~BinaryObject() = default;
  GENERATE_TRIVIAL_METHOD(Type::Binary, Binary, Binary, DO_NOT_GENERATE_HASH);
  size_t hash() const override {
    // TODO(tianqian.zyf:) Reduce CPU overhead associated with hash calculations
    static size_t hash = 0;
    if (hash != 0) {
      return hash;
    }

    hash = data_.size();
    for (auto& i : data_) {
      Utils::hashCombine(hash, i);
    }
    return hash;
  }
  std::string toDebugString() const override {
    // By default, only the first 16 bytes are output
    size_t limit_len = 16;
    std::ostringstream ostream;
    if (data_.size() <= limit_len) {
      limit_len = data_.size();
    }
    for (size_t i = 0; i < limit_len; i++) {
      ostream << std::hex << data_[i] << " ";
    }
    return absl::StrFormat("Type: binary, size[%d], value[%s]", data_.size(),
                           ostream.str());
  }

  Binary::iterator begin() { return data_.begin(); }
  Binary::iterator end() { return data_.end(); }
  Binary::const_iterator begin() const { return data_.cbegin(); }
  Binary::const_iterator end() const { return data_.cend(); }

 private:
  Binary data_;
  DISALLOW_COPY_AND_ASSIGN(BinaryObject);
};

class StringObject : public Object {
 public:
  StringObject(absl::string_view data) : data_(data) {}
  StringObject(std::unique_ptr<std::string>&& data) : data_(std::move(*data)) {}

  // Object
  GENERATE_TRIVIAL_METHOD(Type::String, std::string, String,
                          DO_NOT_GENERATE_HASH);
  size_t hash() const override {
    // Using absl::Hash for StringObject. Then we can find StringObject by the
    // key of type absl::string derectly in the UntypedMap.
    return absl::Hash<absl::string_view>{}(data_);
  }
  std::string toDebugString() const override {
    return absl::StrFormat("Type: string, value[%s]", data_);
  }

  std::string::iterator begin() { return data_.begin(); }
  std::string::iterator end() { return data_.end(); }

  std::string::const_iterator begin() const { return data_.cbegin(); }
  std::string::const_iterator end() const { return data_.cend(); }

 private:
  std::string data_;
  DISALLOW_COPY_AND_ASSIGN(StringObject);
};

class UntypedListObject : public Object {
 public:
  UntypedListObject() = default;
  UntypedListObject(UntypedList&& data) : data_(std::move(data)) {}

  void setUntypedList(UntypedList&& data) { data_ = std::move(data); }

  // Object
  Type type() const override { return Type::UntypedList; }
  ValidToMethod(UntypedList, UntypedList);
  bool equal(const Object& o) const override {
    if (o.type() != type()) {
      return false;
    }

    ABSL_ASSERT(o.toUntypedList().has_value());
    auto o_data = o.toUntypedList().value();
    if (data_.size() != o_data.get().size()) {
      return false;
    }
    for (size_t i = 0; i < data_.size(); i++) {
      if (*data_[i] != *(o_data.get())[i]) {
        return false;
      }
    }
    return true;
  }
  size_t hash() const override {
    // Avoid the hash computation overhead by using a magic string and data size
    // to calculate the hash value and by comparing operators to handle hash
    // conflicts
    size_t hash = std::hash<std::string>{}(UntypedListMagicString);
    Utils::hashCombine(hash, data_.size());
    return hash;
  }
  std::string toDebugString() const override {
    std::ostringstream ostream;
    for (auto& o : data_) {
      ostream << o->toDebugString() << "\n";
    }
    return absl::StrFormat("Type: untypedlist, value[%s]", ostream.str());
  }

  template <typename... _Args>
  void emplace_back(_Args&&... __args) {
    data_.emplace_back(std::forward<_Args>(__args)...);
  }

  UntypedList::iterator begin() { return data_.begin(); }
  UntypedList::iterator end() { return data_.end(); }
  UntypedList::const_iterator begin() const { return data_.cbegin(); }
  UntypedList::const_iterator end() const { return data_.cend(); }

  Object* get(size_t idx) {
    if (idx >= data_.size()) {
      return nullptr;
    }
    return data_[idx].get();
  }

 private:
  UntypedList data_;
  DISALLOW_COPY_AND_ASSIGN(UntypedListObject);
};

class TypedListObject : public Object {
 public:
  TypedListObject() = default;
  TypedListObject(Object::TypedList&& data) : data_(std::move(data)) {}
  TypedListObject(std::string&& type_name, UntypedList&& item)
      : data_(std::move(type_name), std::move(item)) {}

  void setTypedList(Object::TypedList&& data) { data_ = std::move(data); }

  GENERATE_TRIVIAL_METHOD(Type::TypedList, TypedList, TypedList,
                          DO_NOT_GENERATE_HASH)
  size_t hash() const override {
    size_t hash = 0;
    hash = std::hash<std::string>{}(data_.type_name_);
    Utils::hashCombine(hash, data_.values_.size());
    return hash;
  }
  std::string toDebugString() const override {
    std::ostringstream ostream;
    for (auto& o : data_.values_) {
      ostream << o->toDebugString() << "\n";
    }
    return absl::StrFormat("Type: typedlist, type[%s], value[%s]",
                           data_.type_name_, ostream.str());
  }

  template <typename... _Args>
  void emplace_back(_Args&&... __args) {
    data_.values_.emplace_back(std::forward<_Args>(__args)...);
  }

  UntypedList::iterator begin() { return data_.values_.begin(); }
  UntypedList::iterator end() { return data_.values_.end(); }

  UntypedList::const_iterator begin() const { return data_.values_.cbegin(); }
  UntypedList::const_iterator end() const { return data_.values_.cend(); }

  Object* get(size_t idx) {
    if (idx >= data_.values_.size()) {
      return nullptr;
    }
    return data_.values_[idx].get();
  }

  void setType(const std::string& type) { data_.type_name_ = type; }

 private:
  Object::TypedList data_;
  DISALLOW_COPY_AND_ASSIGN(TypedListObject);
};

class TypedMapObject : public Object {
 public:
  TypedMapObject() = default;
  TypedMapObject(TypedMap&& data) : data_(std::move(data)) {}
  TypedMapObject(std::unique_ptr<TypedMap>&& data) : data_(std::move(*data)) {}
  void setTypedMap(TypedMap&& data) { data_ = std::move(data); }

  GENERATE_TRIVIAL_METHOD(Type::TypedMap, TypedMap, TypedMap,
                          DO_NOT_GENERATE_HASH);
  size_t hash() const override {
    size_t hash = std::hash<std::string>{}(data_.type_name_);
    Utils::hashCombine(hash, data_.field_name_and_value_.size());
    return hash;
  }
  std::string toDebugString() const override {
    std::ostringstream ostream;
    for (auto& o : data_.field_name_and_value_) {
      ostream << "key: " << o.first->toDebugString()
              << " value: " << o.second->toDebugString() << "\n";
    }
    return absl::StrFormat("Type: typedmap, type[%s], value[%s]",
                           data_.type_name_, ostream.str());
  }

  template <typename... _Args>
  std::pair<UntypedMap::iterator, bool> emplace(_Args&&... __args) {
    return data_.field_name_and_value_.emplace(std::forward<_Args>(__args)...);
  }

  UntypedMap::iterator begin() { return data_.field_name_and_value_.begin(); }
  UntypedMap::iterator end() { return data_.field_name_and_value_.end(); }
  UntypedMap::const_iterator begin() const {
    return data_.field_name_and_value_.cbegin();
  }
  UntypedMap::const_iterator end() const {
    return data_.field_name_and_value_.cend();
  }
  // TODO(tianqian.zyf): Remove dup implement
  const Object* get(const std::string& key) const {
    auto o = data_.field_name_and_value_.find(key);
    if (o == data_.field_name_and_value_.end()) {
      return nullptr;
    }

    return o->second.get();
  }

  Object* get(const std::string& key) {
    auto o = data_.field_name_and_value_.find(key);
    if (o == data_.field_name_and_value_.end()) {
      return nullptr;
    }

    return o->second.get();
  }

 private:
  TypedMap data_;
  DISALLOW_COPY_AND_ASSIGN(TypedMapObject);
};

class UntypedMapObject : public Object {
 public:
  UntypedMapObject() = default;
  UntypedMapObject(UntypedMap&& data) : data_(std::move(data)) {}
  void setUntypedMap(UntypedMap&& data) { data_ = std::move(data); }

  // Object
  Type type() const override { return Type::UntypedMap; }
  OptConstRef<UntypedMap> toUntypedMap() const override {
    return std::cref(data_);
  }
  OptRef<UntypedMap> toMutableUntypedMap() override { return std::ref(data_); }
  bool equal(const Object& o) const override {
    if (o.type() != type()) {
      return false;
    }

    ABSL_ASSERT(o.toUntypedMap().has_value());
    auto& o_data = o.toUntypedMap().value().get();
    if (data_.size() != o_data.size()) {
      return false;
    }

    for (const auto& elem : data_) {
      auto it = o_data.find(elem.first);
      if (it == o_data.end()) {
        return false;
      }
      if (*(it->second) != *(elem.second)) {
        return false;
      }
    }
    return true;
  }
  size_t hash() const override {
    // The overhead of calculating hash for map type is too high, and the map
    // itself is unordered, so it is difficult to get a stable hash value, so
    // use the magic string and data size to compute hash for map type.
    size_t hash = std::hash<std::string>{}(UntypedMapMagicString);
    Utils::hashCombine(hash, data_.size());
    return hash;
  }
  std::string toDebugString() const override {
    std::ostringstream ostream;
    for (auto& o : data_) {
      ostream << "key: " << o.first->toDebugString()
              << " value: " << o.second->toDebugString() << "\n";
    }
    return absl::StrFormat("Type: untypedmap, value[%s]", ostream.str());
  }

  template <typename... _Args>
  std::pair<UntypedMap::iterator, bool> emplace(_Args&&... __args) {
    return data_.emplace(std::forward<_Args>(__args)...);
  }

  UntypedMap::iterator begin() { return data_.begin(); }
  UntypedMap::iterator end() { return data_.end(); }
  UntypedMap::const_iterator begin() const { return data_.cbegin(); }
  UntypedMap::const_iterator end() const { return data_.cend(); }

  const Object* get(const std::string& key) const {
    auto o = data_.find(key);
    if (o == data_.end()) {
      return nullptr;
    }

    return o->second.get();
  }
  Object* get(const std::string& key) {
    auto o = data_.find(key);
    if (o == data_.end()) {
      return nullptr;
    }

    return o->second.get();
  }

 private:
  UntypedMap data_;
  DISALLOW_COPY_AND_ASSIGN(UntypedMapObject);
};

class ClassInstanceObject : public Object {
 public:
  ClassInstanceObject() = default;
  ClassInstanceObject(ClassInstance&& data) : data_(std::move(data)) {}

  void setClassInstance(ClassInstance&& data) { data_ = std::move(data); }

  // Object
  GENERATE_TRIVIAL_METHOD(Type::Class, ClassInstance, ClassInstance,
                          DO_NOT_GENERATE_HASH)
  size_t hash() const override { return data_.hash(); }
  std::string toDebugString() const override {
    std::ostringstream ostream;
    for (auto& o : data_.data_) {
      ostream << o->toDebugString() << " ";
    }
    return absl::StrFormat("Type: classinstance, def[%s], value[%s]",
                           data_.def_->toDebugString(), ostream.str());
  }

 private:
  ClassInstance data_;
  DISALLOW_COPY_AND_ASSIGN(ClassInstanceObject);
};
}  // namespace Hessian2
