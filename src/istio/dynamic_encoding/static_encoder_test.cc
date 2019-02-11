/* Copyright 2017 Istio Authors. All Rights Reserved.
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
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "encoder.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "message_encoder_builder.h"
#include "src/istio/dynamic_encoding/testdata/types.pb.h"
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

}  // namespace

class StaticEncoderTest : public ::testing::Test {
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

TEST_F(StaticEncoderTest, TestEncoding) {
  ASSERT_NE(GetFileDescriptorSet(), nullptr) << "filedescriptor set is null";

  foo::Simple simple;
  simple.set_flt(float(1.0));
  std::unique_ptr<MessageEncoderBuilder> msgEncoderBuilder =
      absl::make_unique<MessageEncoderBuilder>(GetFileDescriptorSet(), true);
  absl::flat_hash_map<std::string, absl::any> data;
  data["flt"] = float(1.0);

  std::unique_ptr<Encoder> encoder =
      msgEncoderBuilder->Build("foo.Simple", data);
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

}  // namespace dynamic_encoding
}  // namespace istio
