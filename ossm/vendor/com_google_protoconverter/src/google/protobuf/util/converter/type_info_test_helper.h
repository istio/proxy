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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_TEST_HELPER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_TEST_HELPER_H_

#include <memory>
#include <vector>

#include "google/protobuf/util/converter/default_value_objectwriter.h"
#include "google/protobuf/util/converter/protostream_objectsource.h"
#include "google/protobuf/util/converter/protostream_objectwriter.h"
#include "google/protobuf/util/converter/type_info.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/util/type_resolver.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {
namespace testing {

enum TypeInfoSource {
  USE_TYPE_RESOLVER,
};

// In the unit-tests we want to test two scenarios: one with type info from
// ServiceTypeInfo, the other with type info from TypeResolver. This class
// wraps the detail of where the type info is from and provides the same
// interface so the same unit-test code can test both scenarios.
class TypeInfoTestHelper {
 public:
  explicit TypeInfoTestHelper(TypeInfoSource type) : type_(type) {}

  // Creates a TypeInfo object for the given set of descriptors.
  void ResetTypeInfo(const std::vector<const Descriptor*>& descriptors);

  // Convenient overloads.
  void ResetTypeInfo(const Descriptor* descriptor);
  void ResetTypeInfo(const Descriptor* descriptor1,
                     const Descriptor* descriptor2);

  // Returns the TypeInfo created after ResetTypeInfo.
  TypeInfo* GetTypeInfo();

  ProtoStreamObjectSource* NewProtoSource(
      io::CodedInputStream* coded_input, const std::string& type_url,
      ProtoStreamObjectSource::RenderOptions render_options = {});

  ProtoStreamObjectWriter* NewProtoWriter(
      const std::string& type_url, strings::ByteSink* output,
      ErrorListener* listener, const ProtoStreamObjectWriter::Options& options);

  DefaultValueObjectWriter* NewDefaultValueWriter(const std::string& type_url,
                                                  ObjectWriter* writer);

 private:
  TypeInfoSource type_;
  std::unique_ptr<TypeInfo> typeinfo_;
  std::unique_ptr<TypeResolver> type_resolver_;
};
}  // namespace testing
}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_TEST_HELPER_H_
