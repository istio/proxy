// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_messages.h"

#include <optional>

#include "quiche/common/platform/api/quiche_test.h"

namespace moqt::test {
namespace {

TEST(MoqtMessagesTest, MoqtDatagramType) {
  for (bool payload : {false, true}) {
    for (bool extension : {false, true}) {
      for (bool end_of_group : {false, true}) {
        for (bool default_priority : {false, true}) {
          for (bool zero_object_id : {false, true}) {
            MoqtDatagramType type(payload, extension, end_of_group,
                                  default_priority, zero_object_id);
            EXPECT_EQ(type.has_status(),
                      !payload && (!end_of_group || !zero_object_id));
            EXPECT_EQ(type.has_extension(), extension);
            EXPECT_EQ(type.end_of_group(),
                      end_of_group && (payload || zero_object_id));
            EXPECT_EQ(type.has_object_id(),
                      !zero_object_id || (!payload && !end_of_group));
            // The constructor should always produce a valid value.
            std::optional<MoqtDatagramType> from_value =
                MoqtDatagramType::FromValue(type.value());
            EXPECT_TRUE(from_value.has_value() && type == *from_value);
          }
        }
      }
    }
  }
}

}  // namespace
}  // namespace moqt::test
