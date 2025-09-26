// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/protobuf/util/converter/default_value_objectwriter.h"

#include "google/protobuf/util/converter/expecting_objectwriter.h"
#include "absl/strings/cord.h"
#include "google/protobuf/util/converter/constants.h"
#include "google/protobuf/util/converter/testdata/default_value_test.pb.h"
#include "google/protobuf/util/converter/type_info_test_helper.h"
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {
namespace testing {

using proto_util_converter::testing::DefaultValueTest;

// Base class for setting up required state for running default values tests on
// different descriptors.
class BaseDefaultValueObjectWriterTest
    : public ::testing::TestWithParam<testing::TypeInfoSource> {
 protected:
  explicit BaseDefaultValueObjectWriterTest(const Descriptor* descriptor)
      : helper_(GetParam()), mock_(), expects_(&mock_) {
    helper_.ResetTypeInfo(descriptor);
    testing_.reset(helper_.NewDefaultValueWriter(
        absl::StrCat(kTypeServiceBaseUrl, "/", descriptor->full_name()),
        &mock_));
  }

  ~BaseDefaultValueObjectWriterTest() override {}

  TypeInfoTestHelper helper_;
  MockObjectWriter mock_;
  ExpectingObjectWriter expects_;
  std::unique_ptr<DefaultValueObjectWriter> testing_;
};

// Tests to cover some basic DefaultValueObjectWriter use cases. More tests are
// in the marshalling_test.cc and translator_integration_test.cc.
class DefaultValueObjectWriterTest : public BaseDefaultValueObjectWriterTest {
 protected:
  DefaultValueObjectWriterTest()
      : BaseDefaultValueObjectWriterTest(DefaultValueTest::descriptor()) {}
  ~DefaultValueObjectWriterTest() override {}
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         DefaultValueObjectWriterTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(DefaultValueObjectWriterTest, Empty) {
  // Set expectation
  expects_.StartObject("")
      ->RenderDouble("doubleValue", 0.0)
      ->StartList("repeatedDouble")
      ->EndList()
      ->RenderFloat("floatValue", 0.0)
      ->RenderInt64("int64Value", 0)
      ->RenderUint64("uint64Value", 0)
      ->RenderInt32("int32Value", 0)
      ->RenderUint32("uint32Value", 0)
      ->RenderBool("boolValue", false)
      ->RenderString("stringValue", "")
      ->RenderBytes("bytesValue", "")
      ->RenderString("enumValue", "ENUM_FIRST")
      ->EndObject();

  // Actual testing
  testing_->StartObject("")->EndObject();
}

TEST_P(DefaultValueObjectWriterTest, NonDefaultDouble) {
  // Set expectation
  expects_.StartObject("")
      ->RenderDouble("doubleValue", 1.0)
      ->StartList("repeatedDouble")
      ->EndList()
      ->RenderFloat("floatValue", 0.0)
      ->RenderInt64("int64Value", 0)
      ->RenderUint64("uint64Value", 0)
      ->RenderInt32("int32Value", 0)
      ->RenderUint32("uint32Value", 0)
      ->RenderBool("boolValue", false)
      ->RenderString("stringValue", "")
      ->RenderString("enumValue", "ENUM_FIRST")
      ->EndObject();

  // Actual testing
  testing_->StartObject("")->RenderDouble("doubleValue", 1.0)->EndObject();
}

TEST_P(DefaultValueObjectWriterTest, ShouldRetainUnknownField) {
  // Set expectation
  expects_.StartObject("")
      ->RenderDouble("doubleValue", 1.0)
      ->StartList("repeatedDouble")
      ->EndList()
      ->RenderFloat("floatValue", 0.0)
      ->RenderInt64("int64Value", 0)
      ->RenderUint64("uint64Value", 0)
      ->RenderInt32("int32Value", 0)
      ->RenderUint32("uint32Value", 0)
      ->RenderBool("boolValue", false)
      ->RenderString("stringValue", "")
      ->RenderString("unknown", "abc")
      ->StartObject("unknownObject")
      ->RenderString("unknown", "def")
      ->EndObject()
      ->RenderString("enumValue", "ENUM_FIRST")
      ->EndObject();

  // Actual testing
  testing_->StartObject("")
      ->RenderDouble("doubleValue", 1.0)
      ->RenderString("unknown", "abc")
      ->StartObject("unknownObject")
      ->RenderString("unknown", "def")
      ->EndObject()
      ->EndObject();
}


class DefaultValueObjectWriterSuppressListTest
    : public BaseDefaultValueObjectWriterTest {
 protected:
  DefaultValueObjectWriterSuppressListTest()
      : BaseDefaultValueObjectWriterTest(DefaultValueTest::descriptor()) {
    testing_->set_suppress_empty_list(true);
  }
  ~DefaultValueObjectWriterSuppressListTest() override {}
};

INSTANTIATE_TEST_SUITE_P(DifferentTypeInfoSourceTest,
                         DefaultValueObjectWriterSuppressListTest,
                         ::testing::Values(
                             testing::USE_TYPE_RESOLVER));

TEST_P(DefaultValueObjectWriterSuppressListTest, Empty) {
  // Set expectation. Empty lists should be suppressed.
  expects_.StartObject("")
      ->RenderDouble("doubleValue", 0.0)
      ->RenderFloat("floatValue", 0.0)
      ->RenderInt64("int64Value", 0)
      ->RenderUint64("uint64Value", 0)
      ->RenderInt32("int32Value", 0)
      ->RenderUint32("uint32Value", 0)
      ->RenderBool("boolValue", false)
      ->RenderString("stringValue", "")
      ->RenderBytes("bytesValue", "")
      ->RenderString("enumValue", "ENUM_FIRST")
      ->EndObject();

  // Actual testing
  testing_->StartObject("")->EndObject();
}
}  // namespace testing
}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
