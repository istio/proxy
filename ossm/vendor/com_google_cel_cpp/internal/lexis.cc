// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/lexis.h"

#include "absl/base/call_once.h"
#include "absl/base/macros.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"

namespace cel::internal {

namespace {

ABSL_CONST_INIT absl::once_flag reserved_keywords_once_flag = {};
ABSL_CONST_INIT absl::flat_hash_set<absl::string_view>* reserved_keywords =
    nullptr;

void InitializeReservedKeywords() {
  ABSL_ASSERT(reserved_keywords == nullptr);
  reserved_keywords = new absl::flat_hash_set<absl::string_view>();
  reserved_keywords->insert("false");
  reserved_keywords->insert("true");
  reserved_keywords->insert("null");
  reserved_keywords->insert("in");
  reserved_keywords->insert("as");
  reserved_keywords->insert("break");
  reserved_keywords->insert("const");
  reserved_keywords->insert("continue");
  reserved_keywords->insert("else");
  reserved_keywords->insert("for");
  reserved_keywords->insert("function");
  reserved_keywords->insert("if");
  reserved_keywords->insert("import");
  reserved_keywords->insert("let");
  reserved_keywords->insert("loop");
  reserved_keywords->insert("package");
  reserved_keywords->insert("namespace");
  reserved_keywords->insert("return");
  reserved_keywords->insert("var");
  reserved_keywords->insert("void");
  reserved_keywords->insert("while");
}

}  // namespace

bool LexisIsReserved(absl::string_view text) {
  absl::call_once(reserved_keywords_once_flag, InitializeReservedKeywords);
  return reserved_keywords->find(text) != reserved_keywords->end();
}

bool LexisIsIdentifier(absl::string_view text) {
  if (text.empty()) {
    return false;
  }
  char first = text.front();
  if (!absl::ascii_isalpha(first) && first != '_') {
    return false;
  }
  for (size_t index = 1; index < text.size(); index++) {
    if (!absl::ascii_isalnum(text[index]) && text[index] != '_') {
      return false;
    }
  }
  return !LexisIsReserved(text);
}

}  // namespace cel::internal
