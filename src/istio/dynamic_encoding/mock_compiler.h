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

#ifndef ISTIO_DYNAMIC_ENCODING_MOCK_COMPILER_H_
#define ISTIO_DYNAMIC_ENCODING_MOCK_COMPILER_H_

#include "compiler.h"
#include "gmock/gmock.h"

namespace istio {
namespace dynamic_encoding {

// Compiler Class.
class MockCompiler : public Compiler {
 public:
  MOCK_METHOD2(
      Compile,
      ::google::protobuf::util::StatusOr<istio::policy::v1beta1::ValueType>(
          const std::string expr, std::string& compiled_expr));
};
}  // namespace dynamic_encoding
}  // namespace istio

#endif  // ISTIO_DYNAMIC_ENCODING_MOCK_COMPILER_H_
