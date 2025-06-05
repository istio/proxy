// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "../pki/parse_certificate.h"
#include "../pki/input.h"
#include <openssl/base.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<bssl::ParsedDistributionPoint> distribution_points;

  bool success = ParseCrlDistributionPoints(bssl::der::Input(data, size),
                                            &distribution_points);

  if (success) {
    // A valid CRLDistributionPoints must have at least 1 element.
    BSSL_CHECK(!distribution_points.empty());
  }

  return 0;
}
