// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
#include "mojo/core/embedder/embedder.h"  // nogncheck
#endif

int main(int argc, char** argv) {
  gurl_base::TestSuite test_suite(argc, argv);

#if !BUILDFLAG(IS_IOS)
  mojo::core::Init();
#endif

  return gurl_base::LaunchUnitTests(
      argc, argv,
      gurl_base::BindOnce(&gurl_base::TestSuite::Run, gurl_base::Unretained(&test_suite)));
}
