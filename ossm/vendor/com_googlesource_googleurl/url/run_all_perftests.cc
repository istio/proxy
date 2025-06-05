// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/perf_test_suite.h"

int main(int argc, char** argv) {
  gurl_base::PerfTestSuite test_suite(argc, argv);
  return gurl_base::LaunchUnitTestsSerially(
      argc, argv,
      gurl_base::BindOnce(&gurl_base::TestSuite::Run, gurl_base::Unretained(&test_suite)));
}
