// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic smoke test to ensure that GURL works properly.

#include "url/gurl.h"

#include <cstdlib>
#include <iostream>

#define ASSERT_EQ(v1, v2) \
    if ((v1) != (v2)) { \
      std::cerr << "Expected equality of" << std::endl \
        << "  " << #v1 << " (equal to " << (v1) << ")" << std::endl \
        << "and" << std::endl \
        << "  " << #v2 << " (equal to " << (v2) << ")" << std::endl; \
      return 1; \
    }

int main(int argc, char** argv) {
  GURL url("https://example.org/test?foo=bar#section");
  ASSERT_EQ(url.scheme(), "https");
  ASSERT_EQ(url.host(), "example.org");
  ASSERT_EQ(url.EffectiveIntPort(), 443);
  ASSERT_EQ(url.path(), "/test");
  ASSERT_EQ(url.query(), "foo=bar");
  ASSERT_EQ(url.ref(), "section");

  // Ensure ICU is functioning correctly.
  GURL idn_url("https://\xe5\x85\x89.example/");
#ifdef GOOGLEURL_SUPPORTS_IDNA
  ASSERT_EQ(idn_url.spec(), "https://xn--54q.example/");
#else
  ASSERT_EQ(idn_url.is_valid(), false);
#endif

  return 0;
}
