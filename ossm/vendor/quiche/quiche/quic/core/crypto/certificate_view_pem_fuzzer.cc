// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <sstream>
#include <string>

#include "quiche/quic/core/crypto/certificate_view.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"

namespace quic {
namespace {

void DoesNotCrash(const std::string& input) {
  std::stringstream stream(input);

  CertificateView::LoadPemFromStream(&stream);
  stream.seekg(0);
  CertificatePrivateKey::LoadPemFromStream(&stream);
}
FUZZ_TEST(CertificateViewPemFuzzer, DoesNotCrash)
    .WithSeeds(
        fuzztest::ReadFilesFromDirectory(getenv("FUZZER_SEED_CORPUS_DIR")));

}  // namespace
}  // namespace quic
