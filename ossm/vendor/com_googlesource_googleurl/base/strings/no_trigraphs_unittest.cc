// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NoTrigraphs, Basic) {
  EXPECT_EQ("??=", gurl_base::StrCat({"?", "?", "="}));
}
