// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"
#include "quiche/common/structured_headers.h"

namespace quiche {
namespace structured_headers {

void CanParseWithoutCrashing(absl::string_view input) {
  ParseItem(input);
  ParseListOfLists(input);
  ParseList(input);
  ParseDictionary(input);
  ParseParameterisedList(input);
}
FUZZ_TEST(StructuredHeadersFuzzer, CanParseWithoutCrashing)
    .WithDomains(fuzztest::Arbitrary<std::string>());

}  // namespace structured_headers
}  // namespace quiche
