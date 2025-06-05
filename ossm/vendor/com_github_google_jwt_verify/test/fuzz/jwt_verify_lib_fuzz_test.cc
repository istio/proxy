// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "jwt_verify_lib/jwks.h"
#include "jwt_verify_lib/jwt.h"
#include "jwt_verify_lib/verify.h"
#include "src/libfuzzer/libfuzzer_macro.h"
#include "test/fuzz/jwt_verify_lib_fuzz_input.pb.h"

namespace google {
namespace jwt_verify {
namespace {

DEFINE_PROTO_FUZZER(const FuzzInput& input) {
  Jwt jwt;
  auto jwt_status = jwt.parseFromString(input.jwt());

  auto jwks1 = Jwks::createFrom(input.jwks(), Jwks::JWKS);
  auto jwks2 = Jwks::createFrom(input.jwks(), Jwks::PEM);

  if (jwt_status == Status::Ok) {
    if (jwks1->getStatus() == Status::Ok) {
      verifyJwt(jwt, *jwks1);
    }
    if (jwks2->getStatus() == Status::Ok) {
      verifyJwt(jwt, *jwks2);
    }
  }
}

}  // namespace
}  // namespace jwt_verify
}  // namespace google
