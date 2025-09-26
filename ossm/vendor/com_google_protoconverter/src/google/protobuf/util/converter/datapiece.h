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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_DATAPIECE_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_DATAPIECE_H_

#include <cstdint>
#include <string>

#include "absl/log/absl_log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/type.pb.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {
class ProtoWriter;

// Container for a single piece of data together with its data type.
//
// For primitive types (int32, int64, uint32, uint64, double, float, bool),
// the data is stored by value.
//
// For string, an absl::string_view is stored. For Cord, a pointer to Cord is
// stored. Just like absl::string_view, the DataPiece class does not own the
// storage for the actual string or Cord, so it is the user's responsibility to
// guarantee that the underlying storage is still valid when the DataPiece is
// accessed.
class DataPiece {
 public:
  // Identifies data type of the value.
  // These are the types supported by DataPiece.
  enum Type {
    TYPE_INT32 = 1,
    TYPE_INT64 = 2,
    TYPE_UINT32 = 3,
    TYPE_UINT64 = 4,
    TYPE_DOUBLE = 5,
    TYPE_FLOAT = 6,
    TYPE_BOOL = 7,
    TYPE_ENUM = 8,
    TYPE_STRING = 9,
    TYPE_BYTES = 10,
    TYPE_NULL = 11,  // explicit NULL type
  };

  // Constructors and Destructor
  explicit DataPiece(const int32_t value)
      : type_(TYPE_INT32), i32_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const int64_t value)
      : type_(TYPE_INT64), i64_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const uint32_t value)
      : type_(TYPE_UINT32), u32_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const uint64_t value)
      : type_(TYPE_UINT64), u64_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const double value)
      : type_(TYPE_DOUBLE),
        double_(value),
        use_strict_base64_decoding_(false) {}
  explicit DataPiece(const float value)
      : type_(TYPE_FLOAT), float_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const bool value)
      : type_(TYPE_BOOL), bool_(value), use_strict_base64_decoding_(false) {}
  DataPiece(absl::string_view value, bool use_strict_base64_decoding)
      : type_(TYPE_STRING),
        str_(value),
        use_strict_base64_decoding_(use_strict_base64_decoding) {}
  // Constructor for bytes. The second parameter is not used.
  DataPiece(absl::string_view value, bool /*dummy*/,
            bool use_strict_base64_decoding)
      : type_(TYPE_BYTES),
        str_(value),
        use_strict_base64_decoding_(use_strict_base64_decoding) {}

  DataPiece(const DataPiece& r)
      : type_(r.type_),
        str_(r.str_),
        use_strict_base64_decoding_(r.use_strict_base64_decoding_) {}

  DataPiece& operator=(const DataPiece& x) {
    use_strict_base64_decoding_ = x.use_strict_base64_decoding_;
    // TODO(b/265804277): this is officially UB.
    str_ = x.str_;
    type_ = x.type_;
    return *this;
  }

  static DataPiece NullData() { return DataPiece(TYPE_NULL, 0); }

  virtual ~DataPiece() {}

  // Accessors
  Type type() const { return type_; }

  bool use_strict_base64_decoding() { return use_strict_base64_decoding_; }

  absl::string_view str() const {
    ABSL_DLOG_IF(FATAL, type_ != TYPE_STRING) << "Not a string type.";
    return str_;
  }

  // Parses, casts or converts the value stored in the DataPiece into an int32.
  absl::StatusOr<int32_t> ToInt32() const;

  // Parses, casts or converts the value stored in the DataPiece into a uint32.
  absl::StatusOr<uint32_t> ToUint32() const;

  // Parses, casts or converts the value stored in the DataPiece into an int64.
  absl::StatusOr<int64_t> ToInt64() const;

  // Parses, casts or converts the value stored in the DataPiece into a uint64.
  absl::StatusOr<uint64_t> ToUint64() const;

  // Parses, casts or converts the value stored in the DataPiece into a double.
  absl::StatusOr<double> ToDouble() const;

  // Parses, casts or converts the value stored in the DataPiece into a float.
  absl::StatusOr<float> ToFloat() const;

  // Parses, casts or converts the value stored in the DataPiece into a bool.
  absl::StatusOr<bool> ToBool() const;

  // Parses, casts or converts the value stored in the DataPiece into a string.
  absl::StatusOr<std::string> ToString() const;

  // Tries to convert the value contained in this datapiece to string. If the
  // conversion fails, it returns the default_string.
  std::string ValueAsStringOrDefault(absl::string_view default_string) const;

  absl::StatusOr<std::string> ToBytes() const;

 private:
  friend class ProtoWriter;

  // Disallow implicit constructor.
  DataPiece();

  // Helper to create NULL or ENUM types.
  DataPiece(Type type, int32_t val)
      : type_(type), i32_(val), use_strict_base64_decoding_(false) {}

  // Same as the ToEnum() method above but with additional flag to ignore
  // unknown enum values.
  absl::StatusOr<int> ToEnum(const google::protobuf::Enum* enum_type,
                             bool use_lower_camel_for_enums,
                             bool case_insensitive_enum_parsing,
                             bool ignore_unknown_enum_values,
                             bool* is_unknown_enum_value) const;

  // For numeric conversion between
  //     int32, int64, uint32, uint64, double, float and bool
  template <typename To>
  absl::StatusOr<To> GenericConvert() const;

  // For conversion from string to
  //     int32, int64, uint32, uint64, double, float and bool
  template <typename To>
  absl::StatusOr<To> StringToNumber(bool (*func)(absl::string_view, To*)) const;

  // Decodes a base64 string. Returns true on success.
  bool DecodeBase64(absl::string_view src, std::string* dest) const;

  // Data type for this piece of data.
  Type type_;

  // Stored piece of data.
  union {
    int32_t i32_;
    int64_t i64_;
    uint32_t u32_;
    uint64_t u64_;
    double double_;
    float float_;
    bool bool_;
    absl::string_view str_;
  };

  // Uses a stricter version of base64 decoding for byte fields.
  bool use_strict_base64_decoding_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_DATAPIECE_H_
