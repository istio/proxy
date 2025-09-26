// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "grpc_transcoding/request_weaver.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

#include "google/protobuf/type.pb.h"
#include "google/protobuf/util/converter/expecting_objectwriter.h"
#include "grpc_transcoding/status_error_listener.h"
#include "gtest/gtest.h"

namespace google {
namespace grpc {

namespace transcoding {
namespace testing {
namespace {

using google::protobuf::Field;
using ::testing::InSequence;
using ::testing::HasSubstr;

class RequestWeaverTest : public ::testing::Test {
 protected:
  RequestWeaverTest() : mock_(), expect_(&mock_) {}

  void Bind(std::string field_path_str, std::string value) {
    std::vector<std::string> field_names =
        absl::StrSplit(field_path_str, ".", absl::SkipEmpty());
    std::vector<const Field*> field_path;
    for (const auto& n : field_names) {
      fields_.emplace_back(CreateField(n));
      field_path.emplace_back(&fields_.back());
    }
    bindings_.emplace_back(
        RequestWeaver::BindingInfo{field_path, std::move(value)});
  }

  std::unique_ptr<RequestWeaver> Create(bool report_collisions) {
    return std::unique_ptr<RequestWeaver>(new RequestWeaver(
        std::move(bindings_), &mock_, &error_listener_, report_collisions));
  }

  google::protobuf::util::converter::MockObjectWriter mock_;
  google::protobuf::util::converter::ExpectingObjectWriter expect_;
  InSequence seq_;  // all our expectations must be ordered

 private:
  std::vector<RequestWeaver::BindingInfo> bindings_;
  std::list<Field> fields_;
  StatusErrorListener error_listener_;

  Field CreateField(std::string name) {
    Field::Cardinality card;
    if (absl::EndsWith(name, "*")) {
      // we use "*" at the end of the field name to denote a repeated field.
      card = Field::CARDINALITY_REPEATED;
      name.pop_back();
    } else {
      card = Field::CARDINALITY_OPTIONAL;
    }
    Field field;
    field.set_name(name);
    field.set_kind(Field::TYPE_STRING);
    field.set_cardinality(card);
    field.set_number(1);  // dummy number
    return field;
  }
};

TEST_F(RequestWeaverTest, PassThrough) {
  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderString("x", "a");
  expect_.RenderBytes("by", "b");
  expect_.RenderInt32("i", 1);
  expect_.RenderUint32("ui", 2);
  expect_.RenderInt64("i64", 3);
  expect_.RenderUint64("ui64", 4);
  expect_.RenderBool("b", true);
  expect_.RenderNull("null");
  expect_.StartObject("B");
  expect_.RenderString("y", "b");
  expect_.EndObject();  // B
  expect_.EndObject();  // A
  expect_.EndObject();  // ""

  auto w = Create(false);
  w->StartObject("");
  w->StartObject("A");
  w->RenderString("x", "a");
  w->RenderBytes("by", "b");
  w->RenderInt32("i", int32_t(1));
  w->RenderUint32("ui", uint32_t(2));
  w->RenderInt64("i64", int64_t(3));
  w->RenderUint64("ui64", uint64_t(4));
  w->RenderBool("b", true);
  w->RenderNull("null");
  w->StartObject("B");
  w->RenderString("y", "b");
  w->EndObject();
  w->EndObject();
  w->EndObject();

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, Level0Bindings) {
  Bind("_x", "a");
  Bind("_y", "b");
  Bind("_z", "c");

  // {
  //   "i" : "10",
  //   "x" : "d",
  //   ("_x" : "a",)
  //   ("_y" : "b",)
  //   ("_z" : "c",)
  // }

  expect_.StartObject("");
  expect_.RenderInt32("i", 10);
  expect_.RenderString("x", "d");
  expect_.RenderString("_x", "a");
  expect_.RenderString("_y", "b");
  expect_.RenderString("_z", "c");
  expect_.EndObject();

  auto w = Create(false);

  w->StartObject("");
  w->RenderInt32("i", 10);
  w->RenderString("x", "d");
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, Level1Bindings) {
  Bind("A._x", "a");
  Bind("A._y", "b");
  Bind("B._x", "c");

  // {
  //   "x" : "d",
  //   "A" : {
  //     "y" : "e",
  //     ("_x" : "a"),
  //     ("_y" : "b",)
  //   }
  //   "B" : {
  //     "z" : "f",
  //     ("_x" : "c", )
  //   }
  // }

  expect_.StartObject("");
  expect_.RenderString("x", "d");
  expect_.StartObject("A");
  expect_.RenderString("y", "e");
  expect_.RenderString("_x", "a");
  expect_.RenderString("_y", "b");
  expect_.EndObject();  // A
  expect_.StartObject("B");
  expect_.RenderString("z", "f");
  expect_.RenderString("_x", "c");
  expect_.EndObject();  // B
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->RenderString("x", "d");
  w->StartObject("A");
  w->RenderString("y", "e");
  w->EndObject();  // A
  w->StartObject("B");
  w->RenderString("z", "f");
  w->EndObject();  // B
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, Level2Bindings) {
  Bind("A.B._x", "a");
  Bind("A.C._y", "b");
  Bind("D.E._x", "c");

  // {
  //   "A" : {
  //     "B" : {
  //       "x" : "d",
  //       ("_x" : "a",)
  //     },
  //     "y" : "e",
  //     "C" : {
  //       ("_y" : "b",)
  //     }
  //   }
  //   "D" : {
  //     "z" : "f",
  //     "E" : {
  //       "u" : "g",
  //       ("_x" : "c",)
  //     },
  //   }
  // }
  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.StartObject("B");
  expect_.RenderString("x", "d");
  expect_.RenderString("_x", "a");
  expect_.EndObject();  // "B"
  expect_.RenderString("y", "e");
  expect_.StartObject("C");
  expect_.RenderString("_y", "b");
  expect_.EndObject();  // "C"
  expect_.EndObject();  // "A"
  expect_.StartObject("D");
  expect_.RenderString("z", "f");
  expect_.StartObject("E");
  expect_.RenderString("u", "g");
  expect_.RenderString("_x", "c");
  expect_.EndObject();  // "E"
  expect_.EndObject();  // "D"
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->StartObject("A");
  w->StartObject("B");
  w->RenderString("x", "d");
  w->EndObject();  // "B"
  w->RenderString("y", "e");
  w->StartObject("C");
  w->EndObject();  // "C"
  w->EndObject();  // "A"
  w->StartObject("D");
  w->RenderString("z", "f");
  w->StartObject("E");
  w->RenderString("u", "g");
  w->EndObject();  // "E"
  w->EndObject();  // "D"
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, Level2WeaveNewSubTree) {
  Bind("A.B._x", "a");

  // {
  //   "x" : "b",
  //   "C" : {
  //     "y" : "c",
  //     "D" : {
  //       "z" : "c",
  //     }
  //   },
  //   (
  //   "A" {
  //     "B" {
  //      "_x" : "a"
  //     }
  //   }
  //   )
  // }

  expect_.StartObject("");
  expect_.RenderString("x", "b");
  expect_.StartObject("C");
  expect_.RenderString("y", "c");
  expect_.StartObject("D");
  expect_.RenderString("z", "d");
  expect_.EndObject();  // "C"
  expect_.EndObject();  // "D"
  expect_.StartObject("A");
  expect_.StartObject("B");
  expect_.RenderString("_x", "a");
  expect_.EndObject();  // "B"
  expect_.EndObject();  // "A"
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->RenderString("x", "b");
  w->StartObject("C");
  w->RenderString("y", "c");
  w->StartObject("D");
  w->RenderString("z", "d");
  w->EndObject();  // "C"
  w->EndObject();  // "D"
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, MixedBindings) {
  Bind("_x", "a");
  Bind("A.B._y", "b");
  Bind("A._z", "c");

  // {
  //   "A" : {
  //     "x" : "d",
  //     "B" : {
  //       "y" : "e",
  //       ("_y" : "b",)
  //     },
  //     ("_z" : "c",)
  //   },
  //   ("_x" : "a",)
  // }

  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderString("x", "d");
  expect_.StartObject("B");
  expect_.RenderString("y", "e");
  expect_.RenderString("_y", "b");
  expect_.EndObject();  // "B"
  expect_.RenderString("_z", "c");
  expect_.EndObject();  // "A"
  expect_.RenderString("_x", "a");
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->StartObject("A");
  w->RenderString("x", "d");
  w->StartObject("B");
  w->RenderString("y", "e");
  w->EndObject();  // "B"
  w->EndObject();  // "A"
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, MoreMixedBindings) {
  Bind("_x", "a");
  Bind("A._y", "b");
  Bind("B._z", "c");
  Bind("C.D._u", "d");

  // {
  //   "A" : {
  //     "x" : "d",
  //     ("_y" : "b",)
  //   },
  //   "B" : {
  //     "y" : "e",
  //     ("_z" : "c",)
  //   },
  //   ("_x" : "a",)
  //   (
  //   "C" : {
  //     "D" : {
  //       ("_u" : "d",)
  //     },
  //   },
  //   )
  // }

  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderString("x", "d");
  expect_.RenderString("_y", "b");
  expect_.EndObject();  // "A"
  expect_.StartObject("B");
  expect_.RenderString("y", "e");
  expect_.RenderString("_z", "c");
  expect_.EndObject();  // "B"
  expect_.RenderString("_x", "a");
  expect_.StartObject("C");
  expect_.StartObject("D");
  expect_.RenderString("_u", "d");
  expect_.EndObject();  // "D"
  expect_.EndObject();  // "C"
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->StartObject("A");
  w->RenderString("x", "d");
  w->EndObject();  // "A"
  w->StartObject("B");
  w->RenderString("y", "e");
  w->EndObject();  // "B"
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, CollisionIgnored) {
  Bind("A.x", "a");

  // {
  //   "A" : {
  //     "x" : "b",
  //     ("x" : "a") -- ignored
  //   }
  // }

  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderString("x", "b");
  expect_.EndObject();  // "A"
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->StartObject("A");
  w->RenderString("x", "b");
  w->EndObject();  // "A"
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, CollisionReportedInvalidBinding) {
  Bind("A.bool_field", "true1");
  Bind("A.int32_field", "abc");
  Bind("A.uint32_field", "abc");
  Bind("A.int64_field", "abc");
  Bind("A.uint64_field", "abc");
  Bind("A.float_field", "abc");
  Bind("A.double_field", "abc");

  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderBool("bool_field", false);
  expect_.RenderInt32("int32_field", -3);
  expect_.RenderUint32("uint32_field", 3);
  expect_.RenderInt64("int64_field", -3);
  expect_.RenderUint64("uint64_field", 3);
  expect_.RenderFloat("float_field", 1.0001);
  expect_.RenderDouble("double_field", 1.0001);
  expect_.EndObject();  // "A"
  expect_.EndObject();  // ""

  auto w = Create(true);

  w->StartObject("");
  w->StartObject("A");
  w->RenderBool("bool_field", false);
  EXPECT_EQ(w->Status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      w->Status().ToString(),
      HasSubstr(
          "Failed to convert binding value bool_field:\"true1\" to bool"));
  w->RenderInt32("int32_field", -3);
  EXPECT_THAT(
      w->Status().ToString(),
      HasSubstr(
          "Failed to convert binding value int32_field:\"abc\" to int32"));
  w->RenderUint32("uint32_field", 3);
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("Failed to convert binding value "
                        "uint32_field:\"abc\" to uint32"));
  w->RenderInt64("int64_field", -3);
  EXPECT_THAT(
      w->Status().ToString(),
      HasSubstr(
          "Failed to convert binding value int64_field:\"abc\" to int64"));
  w->RenderUint64("uint64_field", 3);
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("Failed to convert binding value "
                        "uint64_field:\"abc\" to uint64"));
  w->RenderFloat("float_field", 1.0001);
  EXPECT_THAT(
      w->Status().ToString(),
      HasSubstr(
          "Failed to convert binding value float_field:\"abc\" to float"));
  w->RenderDouble("double_field", 1.0001);
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("Failed to convert binding value "
                        "double_field:\"abc\" to double"));
  w->EndObject();  // "A"
  w->EndObject();  // ""
}

TEST_F(RequestWeaverTest, CollisionNotReported) {
  Bind("A.bool_field", "true");
  Bind("A.int32_field", "-2");
  Bind("A.uint32_field", "2");
  Bind("A.int64_field", "-2");
  Bind("A.uint64_field", "2");
  Bind("A.string_field", "a");
  Bind("A.float_field", "1.01");
  Bind("A.double_field", "1.01");
  Bind("A.bytes_field", "Yg==");
  Bind("A.B.B_bool_field", "true");

  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderBool("bool_field", true);
  expect_.RenderInt32("int32_field", -2);
  expect_.RenderUint32("uint32_field", 2);
  expect_.RenderInt64("int64_field", -2);
  expect_.RenderUint64("uint64_field", 2);
  expect_.RenderString("string_field", "a");
  expect_.RenderFloat("float_field", 1.01);
  expect_.RenderDouble("double_field", 1.01);
  expect_.RenderBytes("bytes_field", "b");
  expect_.StartObject("B");
  expect_.RenderBool("B_bool_field", true);
  expect_.EndObject();  // "B"
  expect_.EndObject();  // "A"
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->StartObject("A");
  w->RenderBool("bool_field", true);
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->RenderInt32("int32_field", -2);
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->RenderUint32("uint32_field", 2);
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->RenderInt64("int64_field", -2);
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->RenderUint64("uint64_field", 2);
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->RenderString("string_field", "a");
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->RenderFloat("float_field", 1.01);
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->RenderDouble("double_field", 1.01);
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->RenderBytes("bytes_field", "b");
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->StartObject("B");
  w->RenderBool("B_bool_field", true);
  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
  w->EndObject();  // "B"
  w->EndObject();  // "A"
  w->EndObject();  // ""
}

TEST_F(RequestWeaverTest, CollisionReported) {
  Bind("A.bool_field", "true");
  Bind("A.int32_field", "-2");
  Bind("A.uint32_field", "2");
  Bind("A.int64_field", "-2");
  Bind("A.uint64_field", "2");
  Bind("A.string_field", "a");
  Bind("A.float_field", "1.01");
  Bind("A.double_field", "1.01");
  Bind("A.bytes_field", "a");
  Bind("A.B.B_bool_field", "true");

  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderBool("bool_field", false);
  expect_.RenderInt32("int32_field", -3);
  expect_.RenderUint32("uint32_field", 3);
  expect_.RenderInt64("int64_field", -3);
  expect_.RenderUint64("uint64_field", 3);
  expect_.RenderString("string_field", "b");
  expect_.RenderFloat("float_field", 1.0001);
  expect_.RenderDouble("double_field", 1.0001);
  expect_.RenderBytes("bytes_field", "c");
  expect_.StartObject("B");
  expect_.RenderBool("B_bool_field", false);
  expect_.EndObject();  // "B"
  expect_.EndObject();  // "A"
  expect_.EndObject();  // ""

  auto w = Create(true);

  w->StartObject("");
  w->StartObject("A");
  w->RenderBool("bool_field", false);
  EXPECT_EQ(w->Status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("The binding value \"true\" of the field bool_field is "
                        "conflicting with the value false in the body."));
  w->RenderInt32("int32_field", -3);
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("The binding value \"-2\" of the field int32_field is "
                        "conflicting with the value -3 in the body."));
  w->RenderUint32("uint32_field", 3);
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("The binding value \"2\" of the field uint32_field is "
                        "conflicting with the value 3 in the body."));
  w->RenderInt64("int64_field", -3);
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("The binding value \"-2\" of the field int64_field is "
                        "conflicting with the value -3 in the body."));
  w->RenderUint64("uint64_field", 3);
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("The binding value \"2\" of the field uint64_field is "
                        "conflicting with the value 3 in the body."));
  w->RenderString("string_field", "b");
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("The binding value \"a\" of the field string_field is "
                        "conflicting with the value \"b\" in the body."));
  w->RenderFloat("float_field", 1.0001);
  EXPECT_THAT(
      w->Status().ToString(),
      HasSubstr("The binding value \"1.01\" of the field float_field is "
                "conflicting with the value 1.0001 in the body."));
  w->RenderDouble("double_field", 1.0001);
  EXPECT_THAT(
      w->Status().ToString(),
      HasSubstr("The binding value \"1.01\" of the field double_field is "
                "conflicting with the value 1.0001 in the body."));
  w->RenderBytes("bytes_field", "c");
  EXPECT_THAT(w->Status().ToString(),
              HasSubstr("The binding value \"a\" of the field bytes_field is "
                        "conflicting with the value \"c\" in the body."));
  w->StartObject("B");
  w->RenderBool("B_bool_field", false);
  EXPECT_THAT(
      w->Status().ToString(),
      HasSubstr("The binding value \"true\" of the field B_bool_field is "
                "conflicting with the value false in the body."));
  w->EndObject();  // "B"
  w->EndObject();  // "A"
  w->EndObject();  // ""
}

TEST_F(RequestWeaverTest, CollisionRepeated) {
  // "x*" means a repeated field with the name "x"
  Bind("A.x*", "b");
  Bind("A.x*", "c");
  Bind("A.x*", "d");

  // {
  //   "A" : {
  //     "x" : "a",
  //     ("x" : "b")
  //     ("x" : "c")
  //     ("x" : "d")
  //   }
  // }

  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderString("x", "b");
  expect_.RenderString("x", "c");
  expect_.RenderString("x", "d");
  expect_.RenderString("x", "a");
  expect_.EndObject();  // "A"
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->StartObject("A");
  w->RenderString("x", "a");
  w->EndObject();  // "A"
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

TEST_F(RequestWeaverTest, IgnoreListTest) {
  Bind("A._x", "a");

  // {
  //   "L" : [
  //     {
  //       "A" : {
  //         "x" : "b"
  //       },
  //     },
  //   ],
  //   "A" : ["c", "d"]
  //   "A" : {
  //     "y" : "e",
  //     ("_x" : "a"),
  //   },
  // }

  expect_.StartObject("");
  expect_.StartList("L");
  expect_.StartObject("");
  expect_.StartObject("A");
  expect_.RenderString("x", "b");
  expect_.EndObject();  // "A"
  expect_.EndObject();  // ""
  expect_.EndList();    // "L"
  expect_.StartList("A");
  expect_.RenderString("", "c");
  expect_.RenderString("", "d");
  expect_.EndList();  // "A"
  expect_.StartObject("A");
  expect_.RenderString("y", "e");
  expect_.RenderString("_x", "a");
  expect_.EndObject();  // "A"
  expect_.EndObject();  // ""

  auto w = Create(false);

  w->StartObject("");
  w->StartList("L");
  w->StartObject("");
  w->StartObject("A");
  w->RenderString("x", "b");
  w->EndObject();  // "A"
  w->EndObject();  // ""
  w->EndList();    // "L"
  w->StartList("A");
  w->RenderString("", "c");
  w->RenderString("", "d");
  w->EndList();  // "A"
  w->StartObject("A");
  w->RenderString("y", "e");
  w->EndObject();  // "A"
  w->EndObject();  // ""

  EXPECT_EQ(w->Status().code(), absl::StatusCode::kOk);
}

}  // namespace
}  // namespace testing
}  // namespace transcoding

}  // namespace grpc
}  // namespace google
