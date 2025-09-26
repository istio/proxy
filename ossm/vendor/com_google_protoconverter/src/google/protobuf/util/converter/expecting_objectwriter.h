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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_EXPECTING_OBJECTWRITER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_EXPECTING_OBJECTWRITER_H_

// An implementation of ObjectWriter that automatically sets the
// gmock expectations for the response to a method. Every method
// returns the object itself for chaining.
//
// Usage:
//   // Setup
//   MockObjectWriter mock;
//   ExpectingObjectWriter ow(&mock);
//
//   // Set expectation
//   ow.StartObject("")
//       ->RenderString("key", "value")
//     ->EndObject();
//
//   // Actual testing
//   mock.StartObject(absl::string_view())
//         ->RenderString("key", "value")
//       ->EndObject();

#include <cstdint>

#include <gmock/gmock.h>
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/util/converter/object_writer.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

using testing::Eq;
using testing::IsEmpty;
using testing::NanSensitiveDoubleEq;
using testing::NanSensitiveFloatEq;
using testing::Return;
using testing::StrEq;
using testing::TypedEq;

class MockObjectWriter : public ObjectWriter {
 public:
  MockObjectWriter() {}

  MOCK_METHOD(ObjectWriter*, StartObject, (absl::string_view), (override));
  MOCK_METHOD(ObjectWriter*, EndObject, (), (override));
  MOCK_METHOD(ObjectWriter*, StartList, (absl::string_view), (override));
  MOCK_METHOD(ObjectWriter*, EndList, (), (override));
  MOCK_METHOD(ObjectWriter*, RenderBool, (absl::string_view, bool), (override));
  MOCK_METHOD(ObjectWriter*, RenderInt32, (absl::string_view, int32_t),
              (override));
  MOCK_METHOD(ObjectWriter*, RenderUint32, (absl::string_view, uint32_t),
              (override));
  MOCK_METHOD(ObjectWriter*, RenderInt64, (absl::string_view, int64_t),
              (override));
  MOCK_METHOD(ObjectWriter*, RenderUint64, (absl::string_view, uint64_t),
              (override));
  MOCK_METHOD(ObjectWriter*, RenderDouble, (absl::string_view, double),
              (override));
  MOCK_METHOD(ObjectWriter*, RenderFloat, (absl::string_view, float),
              (override));
  MOCK_METHOD(ObjectWriter*, RenderString,
              (absl::string_view, absl::string_view), (override));
  MOCK_METHOD(ObjectWriter*, RenderBytes,
              (absl::string_view, absl::string_view), (override));
  MOCK_METHOD(ObjectWriter*, RenderNull, (absl::string_view), (override));
};

class ExpectingObjectWriter : public ObjectWriter {
 public:
  explicit ExpectingObjectWriter(MockObjectWriter* mock) : mock_(mock) {}
  ExpectingObjectWriter(const ExpectingObjectWriter&) = delete;
  ExpectingObjectWriter& operator=(const ExpectingObjectWriter&) = delete;

  ObjectWriter* StartObject(absl::string_view name) override {
    (name.empty() ? EXPECT_CALL(*mock_, StartObject(IsEmpty()))
                  : EXPECT_CALL(*mock_, StartObject(Eq(std::string(name)))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* EndObject() override {
    EXPECT_CALL(*mock_, EndObject())
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* StartList(absl::string_view name) override {
    (name.empty() ? EXPECT_CALL(*mock_, StartList(IsEmpty()))
                  : EXPECT_CALL(*mock_, StartList(Eq(std::string(name)))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* EndList() override {
    EXPECT_CALL(*mock_, EndList())
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderBool(absl::string_view name, bool value) override {
    (name.empty()
         ? EXPECT_CALL(*mock_, RenderBool(IsEmpty(), TypedEq<bool>(value)))
         : EXPECT_CALL(*mock_,
                       RenderBool(Eq(std::string(name)), TypedEq<bool>(value))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderInt32(absl::string_view name, int32_t value) override {
    (name.empty()
         ? EXPECT_CALL(*mock_, RenderInt32(IsEmpty(), TypedEq<int32_t>(value)))
         : EXPECT_CALL(*mock_, RenderInt32(Eq(std::string(name)),
                                           TypedEq<int32_t>(value))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderUint32(absl::string_view name, uint32_t value) override {
    (name.empty() ? EXPECT_CALL(*mock_, RenderUint32(IsEmpty(),
                                                     TypedEq<uint32_t>(value)))
                  : EXPECT_CALL(*mock_, RenderUint32(Eq(std::string(name)),
                                                     TypedEq<uint32_t>(value))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderInt64(absl::string_view name, int64_t value) override {
    (name.empty()
         ? EXPECT_CALL(*mock_, RenderInt64(IsEmpty(), TypedEq<int64_t>(value)))
         : EXPECT_CALL(*mock_, RenderInt64(Eq(std::string(name)),
                                           TypedEq<int64_t>(value))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderUint64(absl::string_view name, uint64_t value) override {
    (name.empty() ? EXPECT_CALL(*mock_, RenderUint64(IsEmpty(),
                                                     TypedEq<uint64_t>(value)))
                  : EXPECT_CALL(*mock_, RenderUint64(Eq(std::string(name)),
                                                     TypedEq<uint64_t>(value))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderDouble(absl::string_view name, double value) override {
    (name.empty()
         ? EXPECT_CALL(*mock_,
                       RenderDouble(IsEmpty(), NanSensitiveDoubleEq(value)))
         : EXPECT_CALL(*mock_, RenderDouble(Eq(std::string(name)),
                                            NanSensitiveDoubleEq(value))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderFloat(absl::string_view name, float value) override {
    (name.empty()
         ? EXPECT_CALL(*mock_,
                       RenderFloat(IsEmpty(), NanSensitiveFloatEq(value)))
         : EXPECT_CALL(*mock_, RenderFloat(Eq(std::string(name)),
                                           NanSensitiveFloatEq(value))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderString(absl::string_view name,
                             absl::string_view value) override {
    (name.empty() ? EXPECT_CALL(*mock_, RenderString(IsEmpty(),
                                                     TypedEq<absl::string_view>(
                                                         std::string(value))))
                  : EXPECT_CALL(*mock_, RenderString(Eq(std::string(name)),
                                                     TypedEq<absl::string_view>(
                                                         std::string(value)))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }
  ObjectWriter* RenderBytes(absl::string_view name,
                            absl::string_view value) override {
    (name.empty() ? EXPECT_CALL(*mock_, RenderBytes(IsEmpty(),
                                                    TypedEq<absl::string_view>(
                                                        std::string(value))))
                  : EXPECT_CALL(*mock_, RenderBytes(Eq(std::string(name)),
                                                    TypedEq<absl::string_view>(
                                                        std::string(value)))))
        .WillOnce(Return(mock_))
        .RetiresOnSaturation();
    return this;
  }

  ObjectWriter* RenderNull(absl::string_view name) override {
    (name.empty() ? EXPECT_CALL(*mock_, RenderNull(IsEmpty()))
                  : EXPECT_CALL(*mock_, RenderNull(Eq(std::string(name))))
                        .WillOnce(Return(mock_))
                        .RetiresOnSaturation());
    return this;
  }

 private:
  MockObjectWriter* mock_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_EXPECTING_OBJECTWRITER_H_
