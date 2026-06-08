// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/platform/api/quiche_hostname_utils.h"

#include <cstddef>
#include <string>

#include "absl/base/macros.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche {
namespace test {
namespace {

class QuicheHostnameUtilsTest : public QuicheTest {};

TEST_F(QuicheHostnameUtilsTest, IsValidSNI) {
  // IP as SNI.
  EXPECT_FALSE(QuicheHostnameUtils::IsValidSNI("192.168.0.1"));
  // SNI without any dot.
  EXPECT_TRUE(QuicheHostnameUtils::IsValidSNI("somedomain"));
  // Invalid by RFC2396 but unfortunately domains of this form exist.
  EXPECT_TRUE(QuicheHostnameUtils::IsValidSNI("some_domain.com"));
  // An empty string must be invalid otherwise the QUIC client will try sending
  // it.
  EXPECT_FALSE(QuicheHostnameUtils::IsValidSNI(""));

  // Valid SNI
  EXPECT_TRUE(QuicheHostnameUtils::IsValidSNI("test.google.com"));
}

TEST_F(QuicheHostnameUtilsTest, NormalizeHostname) {
  // clang-format off
  struct {
    const char *input, *expected;
  } tests[] = {
      {
          "www.google.com",
          "www.google.com",
      },
      {
          "WWW.GOOGLE.COM",
          "www.google.com",
      },
      {
          "www.google.com.",
          "www.google.com",
      },
      {
          "www.google.COM.",
          "www.google.com",
      },
      {
          "www.google.com..",
          "www.google.com",
      },
      {
          "www.google.com........",
          "www.google.com",
      },
      {
          "",
          "",
      },
      {
          ".",
          "",
      },
      {
          "........",
          "",
      },
  };
  // clang-format on

  for (size_t i = 0; i < ABSL_ARRAYSIZE(tests); ++i) {
    EXPECT_EQ(std::string(tests[i].expected),
              QuicheHostnameUtils::NormalizeHostname(tests[i].input));
  }

  if (GoogleUrlSupportsIdnaForTest()) {
    EXPECT_EQ("xn--54q.google.com", QuicheHostnameUtils::NormalizeHostname(
                                        "\xe5\x85\x89.google.com"));
  } else {
    EXPECT_EQ(
        "", QuicheHostnameUtils::NormalizeHostname("\xe5\x85\x89.google.com"));
  }
}

void FuzzPropertyNormalizationIsIdempotentForValidSni(absl::string_view sni) {
  const bool is_valid = QuicheHostnameUtils::IsValidSNI(sni);
  const std::string normalized = QuicheHostnameUtils::NormalizeHostname(sni);
  const std::string normalized_twice =
      QuicheHostnameUtils::NormalizeHostname(normalized);

  auto GetDebugMessage = [&]() {
    return absl::StrFormat(
        "Original SNI was valid? %v, Original SNI: \"%s\", Normalized SNI: "
        "\"%s\", Double-normalized SNI: \"%s\"",
        is_valid, absl::CEscape(sni), absl::CEscape(normalized),
        absl::CEscape(normalized_twice));
  };

  if (is_valid) {
    // Test idempotency.
    EXPECT_EQ(normalized, normalized_twice) << GetDebugMessage();
    // Test that normalization preserved validity.
    EXPECT_TRUE(QuicheHostnameUtils::IsValidSNI(normalized))
        << GetDebugMessage();
  }
}

FUZZ_TEST(QuicheHostnameUtilsFuzzTest,
          FuzzPropertyNormalizationIsIdempotentForValidSni)
    .WithSeeds({"_._", "...", "!.example", "1.2.3.4"});

}  // namespace
}  // namespace test
}  // namespace quiche
