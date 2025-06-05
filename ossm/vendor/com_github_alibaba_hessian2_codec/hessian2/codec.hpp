#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "hessian2/object.hpp"
#include "hessian2/string_reader.hpp"
#include "hessian2/string_writer.hpp"

namespace Hessian2 {

class Decoder;
class Encoder;

namespace detail {
struct FromHessianFn {
  template <typename CustomType>
  void operator()(CustomType& j, Decoder& c) const noexcept {
    return fromHessian(j, c);
  }
};

struct ToHessianFn {
  template <typename CustomType>
  bool operator()(const CustomType& j, Encoder& e) const noexcept {
    return toHessian(j, e);
  }
};
}  // namespace detail

/// namespace to hold default `fromHessian`/`toHessian` function
/// to see why this is required:
/// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4381.html
namespace {
// Global function objects.
constexpr const auto& FromHessian = static_const<detail::FromHessianFn>::value;
constexpr const auto& ToHessian = static_const<detail::ToHessianFn>::value;
}  // namespace

class Decoder {
 public:
  // Error codes during decode.
  enum class ErrorCode {
    NO_DECODE_ERROR,
    NOT_ENOUGH_BUFFER,
    UNEXPECTED_TYPE,
  };

  // Use default StringReader implement
  Decoder(absl::string_view input)
      : reader_(std::make_unique<StringReader>(input)) {}
  Decoder(ReaderPtr&& read) : reader_(std::move(read)) {}
  template <typename T>
  std::unique_ptr<T> decode() {
    auto t = std::make_unique<T>();
    Hessian2::FromHessian(*t, *this);
    return t;
  }

  uint64_t offset() { return reader_->offset(); }
  uint16_t getTypeRefSize() { return types_ref_.size(); }
  uint16_t getDefRefSize() { return def_ref_.size(); }
  ErrorCode errorCode() const { return error_code_; }
  std::string getErrorMessage() const {
    if (!error_pos_) {
      return absl::StrFormat("pos: %d, %s", error_pos_, errorCodeToString());
    }
    return errorCodeToString();
  }

  int errorPos() const { return error_pos_; }

  const std::vector<Object::RawDefinitionSharedPtr>& getDefRefVec() {
    return def_ref_;
  }

 protected:
  ReaderPtr reader_;
  std::vector<std::string> types_ref_;
  std::vector<Object::RawDefinitionSharedPtr> def_ref_;
  // decode's objects need to have a lifetime longer than value_ref_
  std::vector<Object*> values_ref_;
  ErrorCode error_code_{ErrorCode::NO_DECODE_ERROR};
  int error_pos_{0};

 private:
  std::string errorCodeToString() const {
    switch (error_code_) {
      case ErrorCode::NO_DECODE_ERROR:
        return std::string();
      case ErrorCode::NOT_ENOUGH_BUFFER:
        return std::string("There is not enough buffer");
      case ErrorCode::UNEXPECTED_TYPE:
        return std::string("Unexpected type");
    }
    return std::string();
  }
};

class Encoder {
 public:
  // Error codes during encode.
  enum class ErrorCode {
    NO_ENCODE_ERROR,
  };
  // Use default StringWriter implement
  Encoder(std::string& output)
      : writer_(std::make_unique<StringWriter>(output)) {}
  Encoder(WriterPtr&& writer) : writer_(std::move(writer)) {}
  template <typename T>
  bool encode(const T& o) {
    return Hessian2::ToHessian(o, *this);
  }

  // References are not currently supported
  void encodeVarListBegin(const std::string& type);
  void encodeVarListEnd();

  void encodeFixedListBegin(const std::string& type, uint32_t len);
  void encodeFixedListEnd();

  void encodeMapBegin(const std::string& type);
  void encodeMapEnd();

  void encodeClassInstanceBegin(const Object::RawDefinition&);
  void encodeClassInstanceEnd();

  uint16_t getTypeRefSize() { return types_ref_.size(); }
  uint16_t getDefRefSize() { return def_ref_.size(); }
  uint16_t getValueRefSize() { return values_ref_.size(); }

  int16_t getTypeRef(const std::string& search) {
    auto find = types_ref_.find(search);
    if (find != types_ref_.end()) {
      return find->second;
    }
    return -1;
  }

  int16_t getDefRef(const Object::RawDefinition& def) {
    for (uint16_t i = 0; i < def_ref_.size(); i++) {
      if (*def_ref_[i] == def) {
        return i;
      }
    }
    return -1;
  }

  int16_t getValueRef(const Object* o) {
    auto find = values_ref_.find(o);
    if (find != values_ref_.end()) {
      return find->second;
    }
    return -1;
  }

  ErrorCode errorCode() const { return error_code_; }
  std::string getErrorMessage() const { return errorCodeToString(); }

  const std::vector<Object::RawDefinitionSharedPtr>& getDefRefVec() {
    return def_ref_;
  }

 private:
  std::string errorCodeToString() const {
    switch (error_code_) {
      case ErrorCode::NO_ENCODE_ERROR:
        return std::string();
    }
    return std::string();
  }

 protected:
  WriterPtr writer_;
  std::unordered_map<std::string, uint16_t> types_ref_;
  std::vector<Object::RawDefinitionSharedPtr> def_ref_;
  // Encode's objects need to have a lifetime longer than values_ref_
  // Only two pointers to the same object are considered references
  std::unordered_map<const Object*, uint16_t> values_ref_;
  ErrorCode error_code_{ErrorCode::NO_ENCODE_ERROR};
};

// fwd decl
template <>
std::unique_ptr<Object> Decoder::decode();

template <>
bool Encoder::encode(const Object&);

}  // namespace Hessian2
