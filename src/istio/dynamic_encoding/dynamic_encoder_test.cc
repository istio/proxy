/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <memory>
#include <string>

#include "encoder.h"
#include "message_encoder_builder.h"
#include "src/istio/dynamic_encoding/testdata/types.pb.h"

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "tools/cpp/runfiles/runfiles.h"

namespace istio {
namespace dynamic_encoding {
using bazel::tools::cpp::runfiles::Runfiles;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::util::Status;

namespace {

bool ReadFileToString(const std::string& name, std::string* output) {
  char buffer[1024];
  FILE* file = fopen(name.c_str(), "rb");
  if (file == NULL) return false;

  while (true) {
    size_t n = fread(buffer, 1, sizeof(buffer), file);
    if (n <= 0) break;
    output->append(buffer, n);
  }

  int error = ferror(file);
  if (fclose(file) != 0) return false;
  return error == 0;
}

void ReadDescriptorSet(const std::string& filename,
                       FileDescriptorSet* descriptor_set) {
  std::string file_contents;
  GOOGLE_CHECK_OK(ReadFileToString(filename, &file_contents));
  if (!descriptor_set->ParseFromString(file_contents)) {
    FAIL() << "Could not parse file contents: " << filename;
  }
}

const std::string dmm = R"( {
  str: "mystring"
  i64: response.size | 0
  mapStrStr:
    source_service: source.service | "unknown"
    source_version: source.labels["version"] | "unknown"
  oth:
    inenum: "INNERTHREE"
  enm: request.reason
  si32: -20
  si64: 200000002
  r_enm:
    -0
    - "TWO"
    - connection.sent.bytes
  r_flt:
    -1.12
    - 1.13
  r_i64:
    -response.code
    - 770
})";

const std::string dmmOut = R"( {
  str: mystring
  i64: 200
  mapStrStr:
    source_service: a.svc.cluster.local
    source_version: v1
  oth:
    inenum: INNERTHREE
  enm: TWO
  si32: -20
  si64: 200000002
  r_enm:
    - ONE
    - TWO
    - THREE
  r_flt:
    - 1.12
    - 1.13
  r_i64:
    - 662
    - 770
})";
}  // namespace
// namespace

class DynamicEncoderTest : public ::testing::Test {
 public:
  void SetUp() {
    fileDescriptorSet.reset(new FileDescriptorSet());
    std::string error;
    std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
    if (runfiles.get() == nullptr) {
      FAIL() << error;
    }
    std::string path = runfiles->Rlocation(
        "io_bazel/src/istio/dynamic_encoding/testdata/types.descriptor");
    ReadDescriptorSet("src/istio/dynamic_encoding/testdata/types.descriptor",
                      fileDescriptorSet.get());
  }

  FileDescriptorSet* GetFileDescriptorSet() { return fileDescriptorSet.get(); }

 private:
  std::unique_ptr<FileDescriptorSet> fileDescriptorSet;
};

TEST_F(DynamicEncoderTest, TestStaticPrecoded) {
  ASSERT_NE(GetFileDescriptorSet(), nullptr) << "filedescriptor set is null";

  foo::other oth;
  oth.set_str("foo.Other.Str");
  foo::Simple simple;
  simple.set_str("golden.str");
  *simple.mutable_oth() = oth;

  std::unique_ptr<MessageEncoderBuilder> msgEncoderBuilder =
      absl::make_unique<MessageEncoderBuilder>(GetFileDescriptorSet(), true);

  absl::flat_hash_map<std::string, absl::any> data;
  data["str"] = std::string("\"foo.Other.Str\"");
  std::unique_ptr<Encoder> oth_encoder =
      msgEncoderBuilder->Build("foo.other", data);
  {
    ASSERT_NE(oth_encoder.get(), nullptr) << "encoder set is null";
    MessageEncoder* msgEncoder =
        dynamic_cast<MessageEncoder*>(oth_encoder.get());
    auto status_or_string = msgEncoder->EncodeBytes();
    ASSERT_TRUE(status_or_string.ok());
    absl::any any_val = status_or_string.ValueOrDie();
    std::string* encoded_val = absl::any_cast<std::string>(&any_val);
    ASSERT_NE(encoded_val, nullptr) << "encoded_val set is null";
    foo::other oth2;
    oth2.ParseFromString(*encoded_val);
    GOOGLE_LOG(INFO) << "oth: " << oth.SerializeAsString();
    GOOGLE_LOG(INFO) << "oth2: " << oth2.SerializeAsString();
    EXPECT_TRUE(MessageDifferencer::Equals(oth, oth2));
  }

  absl::flat_hash_map<std::string, absl::any> data2;
  data2["str"] = std::string("\"golden.str\"");
  data2["oth"] = oth_encoder.release();
  std::unique_ptr<Encoder> encoder =
      msgEncoderBuilder->Build("foo.Simple", data2);
  {
    ASSERT_NE(encoder.get(), nullptr) << "encoder set is null";
    MessageEncoder* msgEncoder = dynamic_cast<MessageEncoder*>(encoder.get());
    auto status_or_string = msgEncoder->EncodeBytes();
    ASSERT_TRUE(status_or_string.ok());
    absl::any any_val = status_or_string.ValueOrDie();
    std::string* encoded_val = absl::any_cast<std::string>(&any_val);
    ASSERT_NE(encoded_val, nullptr) << "encoded_val set is null";
    foo::Simple simple2;
    simple2.ParseFromString(*encoded_val);
    EXPECT_TRUE(MessageDifferencer::Equals(simple, simple2));
  }
}
}  // namespace dynamic_encoding
}  // namespace istio
