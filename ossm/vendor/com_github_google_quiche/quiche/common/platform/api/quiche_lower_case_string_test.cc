// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/platform/api/quiche_lower_case_string.h"

#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche::test {
namespace {

TEST(QuicheLowerCaseString, Basic) {
  QuicheLowerCaseString empty("");
  EXPECT_EQ("", empty.get());

  QuicheLowerCaseString from_lower_case("foo");
  EXPECT_EQ("foo", from_lower_case.get());

  QuicheLowerCaseString from_mixed_case("BaR");
  EXPECT_EQ("bar", from_mixed_case.get());

  const absl::string_view kData = "FooBar";
  QuicheLowerCaseString from_string_view(kData);
  EXPECT_EQ("foobar", from_string_view.get());
}

}  // namespace
}  // namespace quiche::test
