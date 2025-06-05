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
#include "grpc_transcoding/response_to_json_translator.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/text_format.h"
#include "grpc_transcoding/type_helper.h"
#include "gtest/gtest.h"
#include "test/bookstore.pb.h"
#include "test_common.h"

namespace google {
namespace grpc {

namespace transcoding {
namespace testing {
namespace {

namespace pbutil = ::google::protobuf::util;

// A helper structure to store a single expected chunk of json and its position
struct ExpectedAt {
  // The position in the input, at which this json is expected
  size_t at;
  std::string json;
};

// ResponseToJsonTranslatorTestRun tests a single ResponseToJsonTranslator
// processing the input as expected.
// It allows feeding chunks of the input (AddChunk()) to the translator and
// testing that the translated messages are generated correctly (Test()).
class ResponseToJsonTranslatorTestRun {
 public:
  // type_resolver - the TypeResolver to be passed to the translator,
  // streaming - whether this is a streaming call or not,
  // type_url - type url of messages being translated,
  // input - the input to be passed to the MessageReader,
  // expected - the expected translated json chunks as the input is processed,
  ResponseToJsonTranslatorTestRun(
      pbutil::TypeResolver* type_resolver, bool streaming,
      const std::string& type_url,
      const JsonResponseTranslateOptions& json_response_translate_options,
      const std::string& input, const std::vector<ExpectedAt>& expected)
      : input_(input),
        expected_(expected),
        streaming_(streaming),
        stream_newline_delimited_(
            json_response_translate_options.stream_newline_delimited),
        input_stream_(new TestZeroCopyInputStream()),
        translator_(new ResponseToJsonTranslator(
            type_resolver, type_url, streaming_, input_stream_.get(),
            json_response_translate_options)),
        position_(0),
        next_expected_(std::begin(expected_)) {}

  // Returns the total input size including the delimiters.
  size_t TotalInputSize() const { return input_.size(); }

  // Adds the next size bytes of input chunk to the input stream, such that the
  // translator can process.
  void AddChunk(size_t size) {
    input_stream_->AddChunk(input_.substr(position_, size));
    position_ += size;
  }

  // Marks the input stream as finished.
  void FinishInputStream() { input_stream_->Finish(); }

  // Tests the ResponseToJsonTranslator at the current position of the input.
  bool Test() {
    // While we still have expected messages before or at the current position
    // try to match.
    while (next_expected_ != std::end(expected_) &&
           next_expected_->at <= position_) {
      // Check the status first
      if (!translator_->Status().ok()) {
        ADD_FAILURE() << "Error: " << translator_->Status().message()
                      << std::endl;
        return false;
      }

      // Read the message
      std::string actual;
      if (!translator_->NextMessage(&actual)) {
        ADD_FAILURE() << "No message available" << std::endl;
        return false;
      }

      // Match the message
      if (streaming_) {
        if (stream_newline_delimited_) {
          if (!ExpectJsonObjectEq(next_expected_->json, actual)) {
            return false;
          }
        } else if (!json_array_tester_.TestElement(next_expected_->json,
                                                   actual)) {
          return false;
        }
      } else {
        if (!ExpectJsonObjectEq(next_expected_->json, actual)) {
          return false;
        }
      }

      // Advance to the next expected message
      ++next_expected_;
    }
    if (input_stream_->Finished() && streaming_) {
      // In case of non-newline-delmited streaming calls if the input is
      // finished, we expect the
      // final ']' at the end of the stream.

      // Read the message
      std::string actual;
      if (!translator_->NextMessage(&actual)) {
        ADD_FAILURE() << "No message available. Missing final ']'" << std::endl;
        return false;
      }

      // Test that it closes the array
      if (!json_array_tester_.TestClosed(actual)) {
        return false;
      }
    }

    // At this point we don't expect any more messages as we read all the ones
    // that must have been available
    std::string actual;
    if (translator_->NextMessage(&actual)) {
      ADD_FAILURE() << "Unexpected message: \"" << actual << "\"" << std::endl;
      return false;
    }

    // Check the status
    if (!translator_->Status().ok()) {
      ADD_FAILURE() << "Error: " << translator_->Status().message()
                    << std::endl;
      return false;
    }

    // Now check that Finished() returns as expected.
    if (translator_->Finished() != input_stream_->Finished()) {
      EXPECT_EQ(input_stream_->Finished(), translator_->Finished());
      return false;
    }

    return true;
  }

 private:
  std::string input_;
  std::vector<ExpectedAt> expected_;
  bool streaming_;
  bool stream_newline_delimited_;

  std::unique_ptr<TestZeroCopyInputStream> input_stream_;
  std::unique_ptr<ResponseToJsonTranslator> translator_;

  // The position in the input string that indicates the part of the input that
  // has already been processed.
  size_t position_;

  // An iterator that points to the next expected message.
  std::vector<ExpectedAt>::const_iterator next_expected_;

  // JsonArrayTester for testing the output JSON array in streaming case
  JsonArrayTester json_array_tester_;
};

// ResponseToJsonTranslatorTestCase tests a single input test case with
// different partitions of the input.
class ResponseToJsonTranslatorTestCase {
 public:
  // type_resolver - the TypeResolver to be passed to the translator,
  // streaming - whether this is a streaming call or not,
  // type_url - type url of messages being translated,
  // input - the input to be passed to the MessageReader,
  // expected - the expected translated json chunks as the input is processed,
  ResponseToJsonTranslatorTestCase(
      pbutil::TypeResolver* type_resolver, bool streaming,
      const std::string& type_url,
      const JsonResponseTranslateOptions& json_response_translate_options,
      std::string input, std::vector<ExpectedAt> expected)
      : type_resolver_(type_resolver),
        streaming_(streaming),
        type_url_(type_url),
        json_response_translate_options_(json_response_translate_options),
        input_(std::move(input)),
        expected_(std::move(expected)) {}

  std::unique_ptr<ResponseToJsonTranslatorTestRun> NewRun() {
    return std::unique_ptr<ResponseToJsonTranslatorTestRun>(
        new ResponseToJsonTranslatorTestRun(
            type_resolver_, streaming_, type_url_,
            json_response_translate_options_, input_, expected_));
  }

  // Runs the test for different partitions of the input.
  // chunk_count - the number of chunks (parts) per partition
  // partitioning_coefficient - defines how exhaustive the test should be. See
  //                            the comment on RunTestForInputPartitions() in
  //                            test_common.h for more details.
  bool Test(size_t chunk_count, double partitioning_coefficient) {
    return RunTestForInputPartitions(chunk_count, partitioning_coefficient,
                                     input_,
                                     [this](const std::vector<size_t>& t) {
                                       auto run = NewRun();

                                       // Feed the chunks according to the
                                       // partition defined by tuple t and
                                       // test along the way.
                                       size_t pos = 0;
                                       for (size_t i = 0; i < t.size(); ++i) {
                                         run->AddChunk(t[i] - pos);
                                         pos = t[i];
                                         if (!run->Test()) {
                                           return false;
                                         }
                                       }
                                       // Feed the last chunk, finish & test.
                                       run->AddChunk(input_.size() - pos);
                                       run->FinishInputStream();
                                       return run->Test();
                                     });
  }

 private:
  pbutil::TypeResolver* type_resolver_;
  bool streaming_;
  std::string type_url_;
  JsonResponseTranslateOptions json_response_translate_options_;

  // The entire input including message delimiters
  std::string input_;

  // Expected JSON chunks
  std::vector<ExpectedAt> expected_;
};

class ResponseToJsonTranslatorTest : public ::testing::Test {
 protected:
  ResponseToJsonTranslatorTest()
      : json_response_translate_options_({pbutil::JsonPrintOptions(), false}) {}

  // Load the service config to be used for testing. This must be the first call
  // in a test.
  bool LoadService(const std::string& config_pb_txt_file) {
    if (!transcoding::testing::LoadService(config_pb_txt_file, &service_)) {
      return false;
    }
    type_helper_.reset(new TypeHelper(service_.types(), service_.enums()));
    return true;
  }

  // Sets the message type for used in this test. Must be used before Build().
  void SetMessageType(const std::string& type_name) {
    type_url_ = "type.googleapis.com/" + type_name;
  }

  // Sets whether to print the streaming response with newline delimiters or
  // not.
  // Must be used before Build(). The default is false;
  void SetJsonStreamNewlineDelimited(bool stream_newline_delimited) {
    json_response_translate_options_.stream_newline_delimited =
        stream_newline_delimited;
  }

  // Sets json print options type for used in this test. Must be used before
  // Build().
  void SetJsonPrintOptions(pbutil::JsonPrintOptions json_print_options) {
    json_response_translate_options_.json_print_options = json_print_options;
  }

  // Sets whether always print primitive fields for default values. Must be used
  // before Build(). The default is false.
  void SetJsonAlwaysPrintPrimitiveFields(bool always_print_primitive_fields) {
    pbutil::JsonPrintOptions json_print_options;
    json_print_options.always_print_primitive_fields =
        always_print_primitive_fields;
    SetJsonPrintOptions(json_print_options);
  }

  // Sets whether this is a streaming call or not. Must be used before Build().
  // The default is non-streaming.
  void SetStreaming(bool streaming) { streaming_ = streaming; }

  // Adds a message to be tested and the corresponding expected JSON. Must be
  // used before Build().
  template <typename MessageType>
  void AddMessage(const std::string& proto_text, std::string expected_json) {
    // Generate a gRPC message and add it to the input
    input_ += GenerateGrpcMessage<MessageType>(proto_text);
    // We will expect expected_json after input.size() bytes are processed.
    expected_.emplace_back(ExpectedAt{input_.size(), expected_json});
  }

  // Builds a ResponseToJsonTranslatorTestCase and resets the input messages in
  // case the test needs to build another one.
  std::unique_ptr<ResponseToJsonTranslatorTestCase> Build() {
    std::string input;
    std::vector<ExpectedAt> expected;
    input.swap(input_);
    expected.swap(expected_);

    return std::unique_ptr<ResponseToJsonTranslatorTestCase>(
        new ResponseToJsonTranslatorTestCase(
            type_helper_->Resolver(), streaming_, type_url_,
            json_response_translate_options_, std::move(input),
            std::move(expected)));
  }

 private:
  ::google::api::Service service_;
  std::unique_ptr<TypeHelper> type_helper_;

  std::string type_url_;
  JsonResponseTranslateOptions json_response_translate_options_;
  bool streaming_ = false;

  // The entire input
  std::string input_;

  // Expected JSON chunks
  std::vector<ExpectedAt> expected_;
};

TEST_F(ResponseToJsonTranslatorTest, Simple) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("Shelf");
  AddMessage<Shelf>(R"(name : "1" theme : "History")",
                    R"({ "name" : "1", "theme" : "History"})");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 1.0));
  EXPECT_TRUE(tc->Test(4, 0.5));
}

TEST_F(ResponseToJsonTranslatorTest, SimpleAlwaysPrintPrimitiveFields) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("Shelf");
  SetJsonAlwaysPrintPrimitiveFields(true);
  AddMessage<Shelf>(R"(name : "" theme : "")",
                    R"({ "name" : "", "theme" : "", "type" : "CLASSIC"})");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 1.0));
  EXPECT_TRUE(tc->Test(4, 0.5));
}

TEST_F(ResponseToJsonTranslatorTest, Nested) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("Book");
  AddMessage<Book>(
      R"(
          name : "8"
          author : "Leo Tolstoy"
          title : "War and Peace"
          author_info {
            first_name : "Leo"
            last_name : "Tolstoy"
            bio {
              year_born : 1830
              year_died : 1910
              text : "some text"
            }
          }
        )",
      R"({
          "author" : "Leo Tolstoy",
          "name" : "8",
          "title" : "War and Peace",
          "authorInfo" : {
            "firstName" : "Leo",
            "lastName" : "Tolstoy",
            "bio" : {
              "yearBorn" : "1830",
              "yearDied" : "1910",
              "text" : "some text"
            }
          }
        })");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 0.2));
}

TEST_F(ResponseToJsonTranslatorTest, StructValueFlat) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("google.protobuf.Struct");
  AddMessage<::google::protobuf::Struct>(
      R"(
        fields {
          key: "payload"
          value { string_value: "Hello World!" }
        })",
      R"({"payload" : "Hello World!"})");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 0.2));
}

TEST_F(ResponseToJsonTranslatorTest, StructValueNested) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("google.protobuf.Struct");
  AddMessage<::google::protobuf::Struct>(
      R"(
        fields {
          key: "nested"
          value {
            struct_value: {
              fields {
                key: "payload"
                value { string_value: "Hello World!" }
              }
            }
          }
        })",
      R"({"nested" : {"payload" : "Hello World!"}})");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 0.2));
}

TEST_F(ResponseToJsonTranslatorTest, NestedAlwaysPrintPrimitiveFields) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("Book");
  SetJsonAlwaysPrintPrimitiveFields(true);
  AddMessage<Book>(
      R"(
          name : ""
          author : ""
          title : ""
          author_info {
            first_name : ""
            last_name : ""
            bio {
              year_born : 0
              year_died : 0
              text : ""
            }
          }
        )",
      R"({
          "author" : "",
          "name" : "",
          "title" : "",
          "authorInfo" : {
            "firstName" : "",
            "lastName" : "",
            "bio" : {
              "yearBorn" : "0",
              "yearDied" : "0",
              "text" : ""
            }
          }
        })");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 0.2));
}

TEST_F(ResponseToJsonTranslatorTest, Empty) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("Shelf");
  AddMessage<Shelf>("", "{}");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
}

TEST_F(ResponseToJsonTranslatorTest, EmptyAlwaysPrintPrimitiveFields) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("Shelf");
  SetJsonAlwaysPrintPrimitiveFields(true);
  AddMessage<Shelf>(R"()",
                    R"({ "name" : "", "theme" : "", "type" : "CLASSIC"})");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
}

TEST_F(ResponseToJsonTranslatorTest, DifferentSizes) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("Shelf");

  auto sizes = {1, 2, 3, 4, 5, 6, 10, 12, 100, 128, 256, 1024, 4096, 65537};
  for (auto size : sizes) {
    auto theme = GenerateInput("abcdefgh12345", size);
    AddMessage<Shelf>(R"(name : "1" theme : ")" + theme + R"(")",
                      R"({ "name" : "1",  "theme" : ")" + theme + R"("})");
    auto tc = Build();
    EXPECT_TRUE(tc->Test(1, 1.0));
  }
}

TEST_F(ResponseToJsonTranslatorTest, StreamingOneMessage) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetStreaming(true);
  SetMessageType("Shelf");
  AddMessage<Shelf>(R"(name : "1" theme : "History")",
                    R"({ "name" : "1", "theme" : "History"})");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 0.5));
  EXPECT_TRUE(tc->Test(4, 0.1));
}

TEST_F(ResponseToJsonTranslatorTest, StreamingThreeMessages) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetStreaming(true);
  SetMessageType("Shelf");
  AddMessage<Shelf>(R"(name : "1" theme : "History")",
                    R"({ "name" : "1", "theme" : "History"})");
  AddMessage<Shelf>(R"(name : "2" theme : "Mistery")",
                    R"({ "name" : "2", "theme" : "Mistery"})");
  AddMessage<Shelf>(R"(name : "3" theme : "Russian")",
                    R"({ "name" : "3", "theme" : "Russian"})");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 0.2));
  EXPECT_TRUE(tc->Test(4, 0.1));
}

TEST_F(ResponseToJsonTranslatorTest, StreamingNoMessages) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetStreaming(true);
  SetMessageType("Shelf");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
}

TEST_F(ResponseToJsonTranslatorTest, StreamingEmptyMessage) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetStreaming(true);
  SetMessageType("Shelf");
  AddMessage<Shelf>("", "{}");
  AddMessage<Shelf>(R"(name : "1" theme : "History")",
                    R"({ "name" : "1", "theme" : "History"})");
  AddMessage<Shelf>("", "{}");
  AddMessage<Shelf>(R"(name : "2" theme : "Classics")",
                    R"({ "name" : "2", "theme" : "Classics"})");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 1.0));
  EXPECT_TRUE(tc->Test(3, 0.2));
}

TEST_F(ResponseToJsonTranslatorTest, Streaming50Messages) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetStreaming(true);
  SetMessageType("Shelf");

  for (size_t i = 1; i <= 50; ++i) {
    auto no = std::to_string(i);
    AddMessage<Shelf>(R"(name : ")" + no +
                          R"(" theme : "th-)" + no + R"(")",
                      R"({ "name" : ")" + no +
                          R"(", "theme" : "th-)" + no + R"("})");
  }

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
}

TEST_F(ResponseToJsonTranslatorTest, StreamingNested) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetStreaming(true);
  SetMessageType("Book");
  AddMessage<Book>(
      R"(
          name : "8"
          author : "Leo Tolstoy"
          title : "War and Peace"
          author_info {
            first_name : "Leo"
            last_name : "Tolstoy"
            bio {
              year_born : 1830
              year_died : 1910
              text : "some text"
            }
          }
        )",
      R"({
          "author" : "Leo Tolstoy",
          "name" : "8",
          "title" : "War and Peace",
          "authorInfo" : {
            "firstName" : "Leo",
            "lastName" : "Tolstoy",
            "bio" : {
              "yearBorn" : "1830",
              "yearDied" : "1910",
              "text" : "some text"
            }
          }
        })");
  AddMessage<Book>(
      R"(
          name : "88"
          author : "Fyodor Dostoevski"
          title : "Crime & Punishment"
          author_info {
            first_name : "Fyodor"
            last_name : "Dostoevski"
            bio {
              year_born : 1840
              year_died : 1920
              text : "some text"
            }
          }
        )",
      R"({
          "author" : "Fyodor Dostoevski",
          "name" : "88",
          "title" : "Crime & Punishment",
          "authorInfo" : {
            "firstName" : "Fyodor",
            "lastName" : "Dostoevski",
            "bio" : {
              "yearBorn" : "1840",
              "yearDied" : "1920",
              "text" : "some text"
            }
          }
        })");

  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
  EXPECT_TRUE(tc->Test(2, 0.3));
  EXPECT_TRUE(tc->Test(3, 0.05));
}

TEST_F(ResponseToJsonTranslatorTest, StreamingDifferentSizes) {
  ASSERT_TRUE(LoadService("bookstore_service.pb.txt"));
  SetMessageType("Shelf");
  SetStreaming(true);

  auto sizes = {1, 2, 3, 4, 5, 6, 10, 12, 100, 128, 256, 1024, 4096, 65537};
  for (auto size : sizes) {
    auto theme = GenerateInput("abcdefgh12345", size);
    AddMessage<Shelf>(R"(name : "1" theme : ")" + theme + R"(")",
                      R"({ "name" : "1",  "theme" : ")" + theme + R"("})");
  }
  auto tc = Build();
  EXPECT_TRUE(tc->Test(1, 1.0));
}

TEST_F(ResponseToJsonTranslatorTest, ErrorInvalidType) {
  // Load the service config
  ::google::api::Service service;
  ASSERT_TRUE(
      transcoding::testing::LoadService("bookstore_service.pb.txt", &service));

  // Create a TypeHelper using the service config
  TypeHelper type_helper(service.types(), service.enums());

  TestZeroCopyInputStream input_stream;
  ResponseToJsonTranslator translator(type_helper.Resolver(),
                                      "type.googleapis.com/InvalidType", false,
                                      &input_stream);

  input_stream.AddChunk(
      GenerateGrpcMessage<Shelf>(R"( name : "1" theme : "Fiction" )"));

  // Call NextMessage() to trigger the error
  std::string message;
  EXPECT_FALSE(translator.NextMessage(&message));
  EXPECT_EQ(absl::StatusCode::kNotFound, translator.Status().code());
}

TEST_F(ResponseToJsonTranslatorTest, DirectTest) {
  // Load the service config
  ::google::api::Service service;
  ASSERT_TRUE(
      transcoding::testing::LoadService("bookstore_service.pb.txt", &service));

  // Create a TypeHelper using the service config
  TypeHelper type_helper(service.types(), service.enums());

  // A message to test
  auto test_message =
      GenerateGrpcMessage<Shelf>(R"(name : "1" theme : "Fiction")");

  TestZeroCopyInputStream input_stream;
  ResponseToJsonTranslator translator(type_helper.Resolver(),
                                      "type.googleapis.com/Shelf", false,
                                      &input_stream);

  std::string message;
  // There is nothing translated
  EXPECT_FALSE(translator.NextMessage(&message));

  // Add the first 10 bytes of the message to the stream
  input_stream.AddChunk(test_message.substr(0, 10));

  // Still nothing
  EXPECT_FALSE(translator.NextMessage(&message));

  // Add the rest of the message to the stream
  input_stream.AddChunk(test_message.substr(10));

  // Now we should have a message
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      ExpectJsonObjectEq(R"({ "name":"1", "theme":"Fiction" })", message));
}

TEST_F(ResponseToJsonTranslatorTest, StreamingDirectTest) {
  // Load the service config
  ::google::api::Service service;
  ASSERT_TRUE(
      transcoding::testing::LoadService("bookstore_service.pb.txt", &service));

  // Create a TypeHelper using the service config
  TypeHelper type_helper(service.types(), service.enums());

  // Messages to test
  auto test_message1 =
      GenerateGrpcMessage<Shelf>(R"(name : "1" theme : "Fiction")");
  auto test_message2 =
      GenerateGrpcMessage<Shelf>(R"(name : "2" theme : "Fantasy")");
  auto test_message3 =
      GenerateGrpcMessage<Shelf>(R"(name : "3" theme : "Children")");
  auto test_message4 =
      GenerateGrpcMessage<Shelf>(R"(name : "4" theme : "Classics")");

  TestZeroCopyInputStream input_stream;
  ResponseToJsonTranslator translator(
      type_helper.Resolver(), "type.googleapis.com/Shelf", true, &input_stream);

  std::string message;
  // There is nothing translated
  EXPECT_FALSE(translator.NextMessage(&message));

  // Add test_message1 to the stream
  input_stream.AddChunk(test_message1);

  JsonArrayTester tester;

  // Now we should have the test_message1 translated
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      tester.TestElement(R"({ "name":"1", "theme":"Fiction" })", message));

  // No more messages, but not finished yet
  EXPECT_FALSE(translator.NextMessage(&message));
  EXPECT_FALSE(translator.Finished());

  // Add the test_message2, test_message3 and part of test_message4
  input_stream.AddChunk(test_message2);
  input_stream.AddChunk(test_message3);
  input_stream.AddChunk(test_message4.substr(0, 10));

  // Now we should have test_message2 & test_message3 translated
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      tester.TestElement(R"({ "name":"2", "theme":"Fantasy" })", message));

  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      tester.TestElement(R"({ "name":"3", "theme":"Children" })", message));

  // No more messages, but not finished yet
  EXPECT_FALSE(translator.NextMessage(&message));
  EXPECT_FALSE(translator.Finished());

  // Add the rest of test_message4
  input_stream.AddChunk(test_message4.substr(10));

  // Now we should have the test_message4 translated
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      tester.TestElement(R"({ "name":"4", "theme":"Classics" })", message));

  // No more messages, but not finished yet
  EXPECT_FALSE(translator.NextMessage(&message));
  EXPECT_FALSE(translator.Finished());

  // Now finish the stream
  input_stream.Finish();

  // Expect the final ']'
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(tester.TestClosed(message));

  // All done!
  EXPECT_FALSE(translator.NextMessage(&message));
  EXPECT_TRUE(translator.Finished());
}

TEST_F(ResponseToJsonTranslatorTest, StreamingNewlineDelimitedDirectTest) {
  // Load the service config
  ::google::api::Service service;
  ASSERT_TRUE(
      transcoding::testing::LoadService("bookstore_service.pb.txt", &service));

  // Create a TypeHelper using the service config
  TypeHelper type_helper(service.types(), service.enums());

  // Messages to test
  auto test_message1 =
      GenerateGrpcMessage<Shelf>(R"(name : "1" theme : "Fiction")");
  auto test_message2 =
      GenerateGrpcMessage<Shelf>(R"(name : "2" theme : "Fantasy")");
  auto test_message3 =
      GenerateGrpcMessage<Shelf>(R"(name : "3" theme : "Children")");
  auto test_message4 =
      GenerateGrpcMessage<Shelf>(R"(name : "4" theme : "Classics")");

  TestZeroCopyInputStream input_stream;
  ResponseToJsonTranslator translator(
      type_helper.Resolver(), "type.googleapis.com/Shelf", true, &input_stream,
      {pbutil::JsonPrintOptions(), true});

  std::string message;
  // There is nothing translated
  EXPECT_FALSE(translator.NextMessage(&message));

  // Add test_message1 to the stream
  input_stream.AddChunk(test_message1);

  // Now we should have the test_message1 translated
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      ExpectJsonObjectEq(R"({ "name":"1", "theme":"Fiction" })", message));
  EXPECT_TRUE(message.back() == '\n');

  // No more messages, but not finished yet
  EXPECT_FALSE(translator.NextMessage(&message));
  EXPECT_FALSE(translator.Finished());

  // Add the test_message2, test_message3 and part of test_message4
  input_stream.AddChunk(test_message2);
  input_stream.AddChunk(test_message3);
  input_stream.AddChunk(test_message4.substr(0, 10));

  // Now we should have test_message2 & test_message3 translated
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      ExpectJsonObjectEq(R"({ "name":"2", "theme":"Fantasy" })", message));
  EXPECT_TRUE(message.back() == '\n');

  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      ExpectJsonObjectEq(R"({ "name":"3", "theme":"Children" })", message));
  EXPECT_TRUE(message.back() == '\n');

  // No more messages, but not finished yet
  EXPECT_FALSE(translator.NextMessage(&message));
  EXPECT_FALSE(translator.Finished());

  // Add the rest of test_message4
  input_stream.AddChunk(test_message4.substr(10));

  // Now we should have the test_message4 translated
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(
      ExpectJsonObjectEq(R"({ "name":"4", "theme":"Classics" })", message));
  EXPECT_TRUE(message.back() == '\n');

  // No more messages, but not finished yet
  EXPECT_FALSE(translator.NextMessage(&message));
  EXPECT_FALSE(translator.Finished());

  // Now finish the stream
  input_stream.Finish();

  // All done!
  EXPECT_TRUE(translator.NextMessage(&message));
  EXPECT_TRUE(translator.Finished());
  EXPECT_FALSE(translator.NextMessage(&message));
}

TEST_F(ResponseToJsonTranslatorTest, Streaming5KMessages) {
  // Load the service config
  ::google::api::Service service;
  ASSERT_TRUE(
      transcoding::testing::LoadService("bookstore_service.pb.txt", &service));

  // Create a TypeHelper using the service config
  TypeHelper type_helper(service.types(), service.enums());

  TestZeroCopyInputStream input_stream;
  ResponseToJsonTranslator translator(
      type_helper.Resolver(), "type.googleapis.com/Shelf", true, &input_stream);

  // Add all messages to the input stream & construct the expected output json
  // array
  std::string expected_json_array = "[";
  std::string actual_json_array;
  for (size_t i = 1; i <= 5000; ++i) {
    auto no = std::to_string(i);

    // Add the message to the input
    input_stream.AddChunk(GenerateGrpcMessage<Shelf>(
        R"(name : ")" + no + R"(" theme : "th-)" + no + R"(")"));

    // Read the translated message
    std::string actual;
    EXPECT_TRUE(translator.NextMessage(&actual));
    actual_json_array += actual;

    // Append the corresponding JSON to the expected array
    if (i > 1) {
      expected_json_array += ",";
    }
    expected_json_array +=
        R"({ "name" : ")" + no + R"(", "theme" : "th-)" + no + R"("})";
  }

  // Close the input stream
  input_stream.Finish();

  // Read the closing ']'
  std::string actual;
  EXPECT_TRUE(translator.NextMessage(&actual));
  actual_json_array += actual;

  // Close the expected array
  expected_json_array += "]";

  // Check the status
  EXPECT_TRUE(translator.Status().ok())
      << "Error " << translator.Status().message() << std::endl;

  // Match the output array
  EXPECT_TRUE(ExpectJsonArrayEq(expected_json_array, actual_json_array));
}

TEST_F(ResponseToJsonTranslatorTest, IncompleteFrameHeader) {
  // Load the service config
  ::google::api::Service service;
  ASSERT_TRUE(
      transcoding::testing::LoadService("bookstore_service.pb.txt", &service));

  // Create a TypeHelper using the service config
  TypeHelper type_helper(service.types(), service.enums());

  TestZeroCopyInputStream input_stream;
  ResponseToJsonTranslator translator(
      type_helper.Resolver(), "type.googleapis.com/Shelf", true, &input_stream);

  input_stream.AddChunk(std::string("\x0A", 1));
  input_stream.Finish();

  std::string actual;
  EXPECT_FALSE(translator.NextMessage(&actual));
  EXPECT_FALSE(translator.Status().ok());
  EXPECT_EQ(translator.Status().message(),
            "Incomplete gRPC frame header received");
}

TEST_F(ResponseToJsonTranslatorTest, InvalidFrameFlag) {
  // Load the service config
  ::google::api::Service service;
  ASSERT_TRUE(
      transcoding::testing::LoadService("bookstore_service.pb.txt", &service));

  // Create a TypeHelper using the service config
  TypeHelper type_helper(service.types(), service.enums());

  TestZeroCopyInputStream input_stream;
  ResponseToJsonTranslator translator(
      type_helper.Resolver(), "type.googleapis.com/Shelf", true, &input_stream);

  input_stream.AddChunk(std::string("\x0A\x00\x00\x00\x00", 5));
  input_stream.Finish();

  std::string actual;
  EXPECT_FALSE(translator.NextMessage(&actual));
  EXPECT_FALSE(translator.Status().ok());
  EXPECT_EQ(translator.Status().message(), "Unsupported gRPC frame flag: 10");
}

TEST_F(ResponseToJsonTranslatorTest, IncompleteFrame) {
  // Load the service config
  ::google::api::Service service;
  ASSERT_TRUE(
      transcoding::testing::LoadService("bookstore_service.pb.txt", &service));

  // Create a TypeHelper using the service config
  TypeHelper type_helper(service.types(), service.enums());

  TestZeroCopyInputStream input_stream;
  ResponseToJsonTranslator translator(
      type_helper.Resolver(), "type.googleapis.com/Shelf", true, &input_stream);

  input_stream.AddChunk(std::string("\x00\x00\x00\x00\x05\x00", 6));
  input_stream.Finish();

  std::string actual;
  EXPECT_FALSE(translator.NextMessage(&actual));
  EXPECT_FALSE(translator.Status().ok());
  EXPECT_EQ(translator.Status().message(),
            "Incomplete gRPC frame expected size: 5 actual size: 1");
}

}  // namespace
}  // namespace testing
}  // namespace transcoding

}  // namespace grpc
}  // namespace google
