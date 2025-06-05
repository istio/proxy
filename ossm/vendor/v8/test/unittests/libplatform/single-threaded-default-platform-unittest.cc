// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-platform.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

template <typename TMixin>
class WithSingleThreadedDefaultPlatformMixin : public TMixin {
 public:
  WithSingleThreadedDefaultPlatformMixin() {
    platform_ = v8::platform::NewSingleThreadedDefaultPlatform();
    CHECK_NOT_NULL(platform_.get());
    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();
  }

  ~WithSingleThreadedDefaultPlatformMixin() override {
    CHECK_NOT_NULL(platform_.get());
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
  }

  v8::Platform* platform() const { return platform_.get(); }

 private:
  std::unique_ptr<v8::Platform> platform_;
};

class SingleThreadedDefaultPlatformTest
    : public WithIsolateScopeMixin<                    //
          WithIsolateMixin<                            //
              WithSingleThreadedDefaultPlatformMixin<  //
                  ::testing::Test>>> {
 public:
  static void SetUpTestSuite() {
    i::FLAG_single_threaded = true;
    i::FlagList::EnforceFlagImplications();
    WithIsolateScopeMixin::SetUpTestSuite();
  }

  static void TearDownTestSuite() {
    WithIsolateScopeMixin::TearDownTestSuite();
  }
};

TEST_F(SingleThreadedDefaultPlatformTest, SingleThreadedDefaultPlatform) {
  {
    i::HandleScope scope(i_isolate());
    v8::Local<Context> env = Context::New(isolate());
    v8::Context::Scope context_scope(env);

    RunJS(
        "function f() {"
        "  for (let i = 0; i < 10; i++)"
        "    (new Array(10)).fill(0);"
        "  return 0;"
        "}"
        "f();");
  }

  CollectGarbage(i::NEW_SPACE);
  CollectAllAvailableGarbage();
}

}  // namespace v8
