#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_VALUE_H_

// CelValue is a holder, capable of storing all kinds of data
// supported by CEL.
// CelValue defines explicitly typed/named getters/setters.
// When storing pointers to objects, CelValue does not accept ownership
// to them and does not control their lifecycle. Instead objects are expected
// to be either external to expression evaluation, and controlled beyond the
// scope or to be allocated and associated with some allocation/ownership
// controller (Arena).
// Usage examples:
// (a) For primitive types:
//    CelValue value = CelValue::CreateInt64(1);
// (b) For string:
//    string* msg = google::protobuf::Arena::Create<string>(arena,"test");
//    CelValue value = CelValue::CreateString(msg);
// (c) For messages:
//    const MyMessage * msg = google::protobuf::Arena::Create<MyMessage>(arena);
//    CelValue value = CelProtoWrapper::CreateMessage(msg, &arena);

#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/kind.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "eval/public/cel_value_internal.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/unknown_set.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "internal/utf8.h"
#include "google/protobuf/message.h"

namespace cel::interop_internal {
struct CelListAccess;
struct CelMapAccess;
}  // namespace cel::interop_internal

namespace google::api::expr::runtime {

using CelError = absl::Status;

// Break cyclic dependencies for container types.
class CelList;
class CelMap;
class LegacyTypeAdapter;

class CelValue {
 public:
  // This class is a container to hold strings/bytes.
  // Template parameter N is an artificial discriminator, used to create
  // distinct types for String and Bytes (we need distinct types for Oneof).
  template <int N>
  class StringHolderBase {
   public:
    StringHolderBase() : value_(absl::string_view()) {}

    StringHolderBase(const StringHolderBase&) = default;
    StringHolderBase& operator=(const StringHolderBase&) = default;

    // string parameter is passed through pointer to ensure string_view is not
    // initialized with string rvalue. Also, according to Google style guide,
    // passing pointers conveys the message that the reference to string is kept
    // in the constructed holder object.
    explicit StringHolderBase(const std::string* str) : value_(*str) {}

    absl::string_view value() const { return value_; }

    // Group of comparison operations.
    friend bool operator==(StringHolderBase value1, StringHolderBase value2) {
      return value1.value_ == value2.value_;
    }

    friend bool operator!=(StringHolderBase value1, StringHolderBase value2) {
      return value1.value_ != value2.value_;
    }

    friend bool operator<(StringHolderBase value1, StringHolderBase value2) {
      return value1.value_ < value2.value_;
    }

    friend bool operator<=(StringHolderBase value1, StringHolderBase value2) {
      return value1.value_ <= value2.value_;
    }

    friend bool operator>(StringHolderBase value1, StringHolderBase value2) {
      return value1.value_ > value2.value_;
    }

    friend bool operator>=(StringHolderBase value1, StringHolderBase value2) {
      return value1.value_ >= value2.value_;
    }

    friend class CelValue;

   private:
    explicit StringHolderBase(absl::string_view other) : value_(other) {}

    absl::string_view value_;
  };

  // Helper structure for String datatype.
  using StringHolder = StringHolderBase<0>;

  // Helper structure for Bytes datatype.
  using BytesHolder = StringHolderBase<1>;

  // Helper structure for CelType datatype.
  using CelTypeHolder = StringHolderBase<2>;

  // Type for CEL Null values. Implemented as a monostate to behave well in
  // absl::variant.
  using NullType = absl::monostate;

  // GCC: fully qualified to avoid change of meaning error.
  using MessageWrapper = google::api::expr::runtime::MessageWrapper;

 private:
  // CelError MUST BE the last in the declaration - it is a ceiling for Type
  // enum
  using ValueHolder = internal::ValueHolder<
      NullType, bool, int64_t, uint64_t, double, StringHolder, BytesHolder,
      MessageWrapper, absl::Duration, absl::Time, const CelList*, const CelMap*,
      const UnknownSet*, CelTypeHolder, const CelError*>;

 public:
  // Metafunction providing positions corresponding to specific
  // types. If type is not supported, compile-time error will occur.
  template <class T>
  using IndexOf = ValueHolder::IndexOf<T>;

  // Enum for types supported.
  // This is not recommended for use in exhaustive switches in client code.
  // Types may be updated over time.
  using Type = ::cel::Kind;

  // Legacy enumeration that is here for testing purposes. Do not use.
  enum class LegacyType {
    kNullType = IndexOf<NullType>::value,
    kBool = IndexOf<bool>::value,
    kInt64 = IndexOf<int64_t>::value,
    kUint64 = IndexOf<uint64_t>::value,
    kDouble = IndexOf<double>::value,
    kString = IndexOf<StringHolder>::value,
    kBytes = IndexOf<BytesHolder>::value,
    kMessage = IndexOf<MessageWrapper>::value,
    kDuration = IndexOf<absl::Duration>::value,
    kTimestamp = IndexOf<absl::Time>::value,
    kList = IndexOf<const CelList*>::value,
    kMap = IndexOf<const CelMap*>::value,
    kUnknownSet = IndexOf<const UnknownSet*>::value,
    kCelType = IndexOf<CelTypeHolder>::value,
    kError = IndexOf<const CelError*>::value,
    kAny  // Special value. Used in function descriptors.
  };

  // Default constructor.
  // Creates CelValue with null data type.
  CelValue() : CelValue(NullType()) {}

  // Returns Type that describes the type of value stored.
  Type type() const { return static_cast<Type>(value_.index()); }

  // Returns debug string describing a value
  const std::string DebugString() const;

  // We will use factory methods instead of public constructors
  // The reason for this is the high risk of implicit type conversions
  // between bool/int/pointer types.
  // We rely on copy elision to avoid extra copying.
  static CelValue CreateNull() { return CelValue(NullType()); }

  // Transitional factory for migrating to null types.
  static CelValue CreateNullTypedValue() { return CelValue(NullType()); }

  static CelValue CreateBool(bool value) { return CelValue(value); }

  static CelValue CreateInt64(int64_t value) { return CelValue(value); }

  static CelValue CreateUint64(uint64_t value) { return CelValue(value); }

  static CelValue CreateDouble(double value) { return CelValue(value); }

  static CelValue CreateString(StringHolder holder) {
    ABSL_ASSERT(::cel::internal::Utf8IsValid(holder.value()));
    return CelValue(holder);
  }

  // Returns a string value from a string_view. Warning: the caller is
  // responsible for the lifecycle of the backing string. Prefer CreateString
  // instead.
  static CelValue CreateStringView(absl::string_view value) {
    return CelValue(StringHolder(value));
  }

  static CelValue CreateString(const std::string* str) {
    return CelValue(StringHolder(str));
  }

  static CelValue CreateBytes(BytesHolder holder) { return CelValue(holder); }

  static CelValue CreateBytesView(absl::string_view value) {
    return CelValue(BytesHolder(value));
  }

  static CelValue CreateBytes(const std::string* str) {
    return CelValue(BytesHolder(str));
  }

  static CelValue CreateDuration(absl::Duration value);

  static CelValue CreateUncheckedDuration(absl::Duration value) {
    return CelValue(value);
  }

  static CelValue CreateTimestamp(absl::Time value) { return CelValue(value); }

  static CelValue CreateList(const CelList* value) {
    CheckNullPointer(value, Type::kList);
    return CelValue(value);
  }

  // Creates a CelValue backed by an empty immutable list.
  static CelValue CreateList();

  static CelValue CreateMap(const CelMap* value) {
    CheckNullPointer(value, Type::kMap);
    return CelValue(value);
  }

  // Creates a CelValue backed by an empty immutable map.
  static CelValue CreateMap();

  static CelValue CreateUnknownSet(const UnknownSet* value) {
    CheckNullPointer(value, Type::kUnknownSet);
    return CelValue(value);
  }

  static CelValue CreateCelType(CelTypeHolder holder) {
    return CelValue(holder);
  }

  static CelValue CreateCelTypeView(absl::string_view value) {
    // This factory method is used for dealing with string references which
    // come from protobuf objects or other containers which promise pointer
    // stability. In general, this is a risky method to use and should not
    // be invoked outside the core CEL library.
    return CelValue(CelTypeHolder(value));
  }

  static CelValue CreateError(const CelError* value) {
    CheckNullPointer(value, Type::kError);
    return CelValue(value);
  }

  // Returns an absl::OkStatus() when the key is a valid protobuf map type,
  // meaning it is a scalar value that is neither floating point nor bytes.
  static absl::Status CheckMapKeyType(const CelValue& key);

  // Obtain the CelType of the value.
  CelValue ObtainCelType() const;

  // Methods for accessing values of specific type
  // They have the common usage pattern - prior to accessing the
  // value, the caller should check that the value of this type is indeed
  // stored in CelValue, using type() or Is...() methods.

  // Returns stored boolean value.
  // Fails if stored value type is not boolean.
  bool BoolOrDie() const { return GetValueOrDie<bool>(Type::kBool); }

  // Returns stored int64 value.
  // Fails if stored value type is not int64.
  int64_t Int64OrDie() const { return GetValueOrDie<int64_t>(Type::kInt64); }

  // Returns stored uint64 value.
  // Fails if stored value type is not uint64.
  uint64_t Uint64OrDie() const {
    return GetValueOrDie<uint64_t>(Type::kUint64);
  }

  // Returns stored double value.
  // Fails if stored value type is not double.
  double DoubleOrDie() const { return GetValueOrDie<double>(Type::kDouble); }

  // Returns stored const string* value.
  // Fails if stored value type is not const string*.
  StringHolder StringOrDie() const {
    return GetValueOrDie<StringHolder>(Type::kString);
  }

  BytesHolder BytesOrDie() const {
    return GetValueOrDie<BytesHolder>(Type::kBytes);
  }

  // Returns stored const Message* value.
  // Fails if stored value type is not const Message*.
  const google::protobuf::Message* MessageOrDie() const {
    MessageWrapper wrapped = MessageWrapperOrDie();
    ABSL_ASSERT(wrapped.HasFullProto());
    return static_cast<const google::protobuf::Message*>(wrapped.message_ptr());
  }

  ABSL_DEPRECATED("Use MessageOrDie")
  MessageWrapper MessageWrapperOrDie() const {
    return GetValueOrDie<MessageWrapper>(Type::kMessage);
  }

  // Returns stored duration value.
  // Fails if stored value type is not duration.
  const absl::Duration DurationOrDie() const {
    return GetValueOrDie<absl::Duration>(Type::kDuration);
  }

  // Returns stored timestamp value.
  // Fails if stored value type is not timestamp.
  const absl::Time TimestampOrDie() const {
    return GetValueOrDie<absl::Time>(Type::kTimestamp);
  }

  // Returns stored const CelList* value.
  // Fails if stored value type is not const CelList*.
  const CelList* ListOrDie() const {
    return GetValueOrDie<const CelList*>(Type::kList);
  }

  // Returns stored const CelMap * value.
  // Fails if stored value type is not const CelMap *.
  const CelMap* MapOrDie() const {
    return GetValueOrDie<const CelMap*>(Type::kMap);
  }

  // Returns stored const CelTypeHolder value.
  // Fails if stored value type is not CelTypeHolder.
  CelTypeHolder CelTypeOrDie() const {
    return GetValueOrDie<CelTypeHolder>(Type::kCelType);
  }

  // Returns stored const UnknownAttributeSet * value.
  // Fails if stored value type is not const UnknownAttributeSet *.
  const UnknownSet* UnknownSetOrDie() const {
    return GetValueOrDie<const UnknownSet*>(Type::kUnknownSet);
  }

  // Returns stored const CelError * value.
  // Fails if stored value type is not const CelError *.
  const CelError* ErrorOrDie() const {
    return GetValueOrDie<const CelError*>(Type::kError);
  }

  bool IsNull() const { return value_.template Visit<bool>(NullCheckOp()); }

  bool IsBool() const { return value_.is<bool>(); }

  bool IsInt64() const { return value_.is<int64_t>(); }

  bool IsUint64() const { return value_.is<uint64_t>(); }

  bool IsDouble() const { return value_.is<double>(); }

  bool IsString() const { return value_.is<StringHolder>(); }

  bool IsBytes() const { return value_.is<BytesHolder>(); }

  bool IsMessage() const { return value_.is<MessageWrapper>(); }

  bool IsDuration() const { return value_.is<absl::Duration>(); }

  bool IsTimestamp() const { return value_.is<absl::Time>(); }

  bool IsList() const { return value_.is<const CelList*>(); }

  bool IsMap() const { return value_.is<const CelMap*>(); }

  bool IsUnknownSet() const { return value_.is<const UnknownSet*>(); }

  bool IsCelType() const { return value_.is<CelTypeHolder>(); }

  bool IsError() const { return value_.is<const CelError*>(); }

  // Invokes op() with the active value, and returns the result.
  // All overloads of op() must have the same return type.
  // Note: this depends on the internals of CelValue, so use with caution.
  template <class ReturnType, class Op>
  ReturnType InternalVisit(Op&& op) const {
    return value_.template Visit<ReturnType>(std::forward<Op>(op));
  }

  // Invokes op() with the active value, and returns the result.
  // All overloads of op() must have the same return type.
  // TODO(uncreated-issue/2): Move to CelProtoWrapper to retain the assumed
  // google::protobuf::Message variant version behavior for client code.
  template <class ReturnType, class Op>
  ReturnType Visit(Op&& op) const {
    return value_.template Visit<ReturnType>(
        internal::MessageVisitAdapter<Op, ReturnType>(std::forward<Op>(op)));
  }

  // Template-style getter.
  // Returns true, if assignment successful
  template <typename Arg>
  bool GetValue(Arg* value) const {
    return this->template InternalVisit<bool>(AssignerOp<Arg>(value));
  }

  // Provides type names for internal logging.
  static std::string TypeName(Type value_type);

  // Factory for message wrapper. This should only be used by internal
  // libraries.
  // TODO(uncreated-issue/2): exposed for testing while wiring adapter APIs. Should
  // make private visibility after refactors are done.
  ABSL_DEPRECATED("Use CelProtoWrapper::CreateMessage")
  static CelValue CreateMessageWrapper(MessageWrapper value) {
    CheckNullPointer(value.message_ptr(), Type::kMessage);
    CheckNullPointer(value.legacy_type_info(), Type::kMessage);
    return CelValue(value);
  }

 private:
  ValueHolder value_;

  template <typename T, class = void>
  struct AssignerOp {
    explicit AssignerOp(T* val) : value(val) {}

    template <typename U>
    bool operator()(const U&) {
      return false;
    }

    bool operator()(const T& arg) {
      *value = arg;
      return true;
    }

    T* value;
  };

  // Specialization for MessageWrapper to support legacy behavior while
  // migrating off hard dependency on google::protobuf::Message.
  // TODO(uncreated-issue/2): Move to CelProtoWrapper.
  template <typename T>
  struct AssignerOp<
      T, std::enable_if_t<std::is_same_v<T, const google::protobuf::Message*>>> {
    explicit AssignerOp(const google::protobuf::Message** val) : value(val) {}

    template <typename U>
    bool operator()(const U&) {
      return false;
    }

    bool operator()(const MessageWrapper& held_value) {
      if (!held_value.HasFullProto()) {
        return false;
      }

      *value = static_cast<const google::protobuf::Message*>(held_value.message_ptr());
      return true;
    }

    const google::protobuf::Message** value;
  };

  struct NullCheckOp {
    template <typename T>
    bool operator()(const T&) const {
      return false;
    }

    bool operator()(NullType) const { return true; }
    // Note: this is not typically possible, but is supported for allowing
    // function resolution for null ptrs as Messages.
    bool operator()(const MessageWrapper& arg) const {
      return arg.message_ptr() == nullptr;
    }
  };

  // Constructs CelValue wrapping value supplied as argument.
  // Value type T should be supported by specification of ValueHolder.
  template <class T>
  explicit CelValue(T value) : value_(value) {}

  // Crashes with a null pointer error.
  static void CrashNullPointer(Type type) ABSL_ATTRIBUTE_COLD {
    ABSL_LOG(FATAL) << "Null pointer supplied for "
                    << TypeName(type);  // Crash ok
  }

  // Null pointer checker for pointer-based types.
  static void CheckNullPointer(const void* ptr, Type type) {
    if (ABSL_PREDICT_FALSE(ptr == nullptr)) {
      CrashNullPointer(type);
    }
  }

  // Crashes with a type mismatch error.
  static void CrashTypeMismatch(Type requested_type,
                                Type actual_type) ABSL_ATTRIBUTE_COLD {
    ABSL_LOG(FATAL) << "Type mismatch"                             // Crash ok
                    << ": expected " << TypeName(requested_type)   // Crash ok
                    << ", encountered " << TypeName(actual_type);  // Crash ok
  }

  // Gets value of type specified
  template <class T>
  T GetValueOrDie(Type requested_type) const {
    auto value_ptr = value_.get<T>();
    if (ABSL_PREDICT_FALSE(value_ptr == nullptr)) {
      CrashTypeMismatch(requested_type, type());
    }
    return *value_ptr;
  }

  friend class CelProtoWrapper;
  friend class ProtoMessageTypeAdapter;
  friend class EvaluatorStack;
  friend class TestOnly_FactoryAccessor;
};

static_assert(absl::is_trivially_destructible<CelValue>::value,
              "Non-trivially-destructible CelValue impacts "
              "performance");

// CelList is a base class for list adapting classes.
class CelList {
 public:
  ABSL_DEPRECATED(
      "Unless you are sure of the underlying CelList implementation, call Get "
      "and pass an arena instead")
  virtual CelValue operator[](int index) const = 0;

  // Like `operator[](int)` above, but also accepts an arena. Prefer calling
  // this variant if the arena is known.
  virtual CelValue Get(google::protobuf::Arena* arena, int index) const {
    static_cast<void>(arena);
    return (*this)[index];
  }

  // List size
  virtual int size() const = 0;
  // Default empty check. Can be overridden in subclass for performance.
  virtual bool empty() const { return size() == 0; }

  virtual ~CelList() {}

 private:
  friend struct cel::interop_internal::CelListAccess;
  friend struct cel::NativeTypeTraits<CelList>;

  virtual cel::NativeTypeId GetNativeTypeId() const {
    return cel::NativeTypeId();
  }
};

// CelMap is a base class for map accessors.
class CelMap {
 public:
  // Map lookup. If value found, returns CelValue in return type.
  //
  // Per the protobuf specification, acceptable key types are bool, int64,
  // uint64, string. Any key type that is not supported should result in valued
  // response containing an absl::StatusCode::kInvalidArgument wrapped as a
  // CelError.
  //
  // Type specializations are permitted since CEL supports such distinctions
  // at type-check time. For example, the expression `1 in map_str` where the
  // variable `map_str` is of type map(string, string) will yield a type-check
  // error. To be consistent, the runtime should also yield an invalid argument
  // error if the type does not agree with the expected key types held by the
  // container.
  // TODO(issues/122): Make this method const correct.
  ABSL_DEPRECATED(
      "Unless you are sure of the underlying CelMap implementation, call Get "
      "and pass an arena instead")
  virtual absl::optional<CelValue> operator[](CelValue key) const = 0;

  // Like `operator[](CelValue)` above, but also accepts an arena. Prefer
  // calling this variant if the arena is known.
  virtual absl::optional<CelValue> Get(google::protobuf::Arena* arena,
                                       CelValue key) const {
    static_cast<void>(arena);
    return (*this)[key];
  }

  // Return whether the key is present within the map.
  //
  // Typically, key resolution will be a simple boolean result; however, there
  // are scenarios where the conversion of the input key to the underlying
  // key-type will produce an absl::StatusCode::kInvalidArgument.
  //
  // Evaluators are responsible for handling non-OK results by propagating the
  // error, as appropriate, up the evaluation stack either as a `StatusOr` or
  // as a `CelError` value, depending on the context.
  virtual absl::StatusOr<bool> Has(const CelValue& key) const {
    // This check safeguards against issues with invalid key types such as NaN.
    CEL_RETURN_IF_ERROR(CelValue::CheckMapKeyType(key));
    google::protobuf::Arena arena;
    auto value = (*this).Get(&arena, key);
    if (!value.has_value()) {
      return false;
    }
    // This protects from issues that may occur when looking up a key value,
    // such as a failure to convert an int64 to an int32 map key.
    if (value->IsError()) {
      return *value->ErrorOrDie();
    }
    return true;
  }

  // Map size
  virtual int size() const = 0;
  // Default empty check. Can be overridden in subclass for performance.
  virtual bool empty() const { return size() == 0; }

  // Return list of keys. CelList is owned by Arena, so no
  // ownership is passed.
  ABSL_DEPRECATED(
      "Unless you are sure of the underlying CelMap implementation, call "
      "ListKeys and pass an arena instead")
  virtual absl::StatusOr<const CelList*> ListKeys() const = 0;

  // Like `ListKeys()` above, but also accepts an arena. Prefer calling this
  // variant if the arena is known.
  virtual absl::StatusOr<const CelList*> ListKeys(google::protobuf::Arena* arena) const {
    static_cast<void>(arena);
    return ListKeys();
  }

  virtual ~CelMap() {}

 private:
  friend struct cel::interop_internal::CelMapAccess;
  friend struct cel::NativeTypeTraits<CelMap>;

  virtual cel::NativeTypeId GetNativeTypeId() const {
    return cel::NativeTypeId();
  }
};

// Utility method that generates CelValue containing CelError.
// message an error message
// error_code error code
CelValue CreateErrorValue(
    cel::MemoryManagerRef manager ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::string_view message,
    absl::StatusCode error_code = absl::StatusCode::kUnknown);
CelValue CreateErrorValue(
    google::protobuf::Arena* arena, absl::string_view message,
    absl::StatusCode error_code = absl::StatusCode::kUnknown);

// Utility method for generating a CelValue from an absl::Status.
CelValue CreateErrorValue(cel::MemoryManagerRef manager
                              ABSL_ATTRIBUTE_LIFETIME_BOUND,
                          const absl::Status& status);

// Utility method for generating a CelValue from an absl::Status.
CelValue CreateErrorValue(google::protobuf::Arena* arena, const absl::Status& status);

// Create an error for failed overload resolution, optionally including the name
// of the function.
CelValue CreateNoMatchingOverloadError(cel::MemoryManagerRef manager
                                           ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                       absl::string_view fn = "");
ABSL_DEPRECATED("Prefer using the generic MemoryManager overload")
CelValue CreateNoMatchingOverloadError(google::protobuf::Arena* arena,
                                       absl::string_view fn = "");
bool CheckNoMatchingOverloadError(CelValue value);

CelValue CreateNoSuchFieldError(cel::MemoryManagerRef manager
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                absl::string_view field = "");
ABSL_DEPRECATED("Prefer using the generic MemoryManager overload")
CelValue CreateNoSuchFieldError(google::protobuf::Arena* arena,
                                absl::string_view field = "");

CelValue CreateNoSuchKeyError(cel::MemoryManagerRef manager
                                  ABSL_ATTRIBUTE_LIFETIME_BOUND,
                              absl::string_view key);
ABSL_DEPRECATED("Prefer using the generic MemoryManager overload")
CelValue CreateNoSuchKeyError(google::protobuf::Arena* arena, absl::string_view key);

bool CheckNoSuchKeyError(CelValue value);

// Returns an error indicating that evaluation has accessed an attribute whose
// value is undefined. For example, this may represent a field in a proto
// message bound to the activation whose value can't be determined by the
// hosting application.
CelValue CreateMissingAttributeError(cel::MemoryManagerRef manager
                                         ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                     absl::string_view missing_attribute_path);
ABSL_DEPRECATED("Prefer using the generic MemoryManager overload")
CelValue CreateMissingAttributeError(google::protobuf::Arena* arena,
                                     absl::string_view missing_attribute_path);

ABSL_CONST_INIT extern const absl::string_view kPayloadUrlMissingAttributePath;
bool IsMissingAttributeError(const CelValue& value);

// Returns error indicating the result of the function is unknown. This is used
// as a signal to create an unknown set if unknown function handling is opted
// into.
CelValue CreateUnknownFunctionResultError(cel::MemoryManagerRef manager
                                              ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                          absl::string_view help_message);
ABSL_DEPRECATED("Prefer using the generic MemoryManager overload")
CelValue CreateUnknownFunctionResultError(google::protobuf::Arena* arena,
                                          absl::string_view help_message);

// Returns true if this is unknown value error indicating that evaluation
// called an extension function whose value is unknown for the given args.
// This is used as a signal to convert to an UnknownSet if the behavior is opted
// into.
bool IsUnknownFunctionResult(const CelValue& value);

}  // namespace google::api::expr::runtime

namespace cel {

template <>
struct NativeTypeTraits<google::api::expr::runtime::CelList> final {
  static NativeTypeId Id(const google::api::expr::runtime::CelList& cel_list) {
    return cel_list.GetNativeTypeId();
  }
};

template <typename T>
struct NativeTypeTraits<
    T,
    std::enable_if_t<std::conjunction_v<
        std::is_base_of<google::api::expr::runtime::CelList, T>,
        std::negation<std::is_same<T, google::api::expr::runtime::CelList>>>>>
    final {
  static NativeTypeId Id(const google::api::expr::runtime::CelList& cel_list) {
    return NativeTypeTraits<google::api::expr::runtime::CelList>::Id(cel_list);
  }
};

template <>
struct NativeTypeTraits<google::api::expr::runtime::CelMap> final {
  static NativeTypeId Id(const google::api::expr::runtime::CelMap& cel_map) {
    return cel_map.GetNativeTypeId();
  }
};

template <typename T>
struct NativeTypeTraits<
    T, std::enable_if_t<std::conjunction_v<
           std::is_base_of<google::api::expr::runtime::CelMap, T>,
           std::negation<std::is_same<T, google::api::expr::runtime::CelMap>>>>>
    final {
  static NativeTypeId Id(const google::api::expr::runtime::CelMap& cel_map) {
    return NativeTypeTraits<google::api::expr::runtime::CelMap>::Id(cel_map);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_VALUE_H_
