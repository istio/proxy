// Copyright 2018 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "examples/xplatform/c_from_swift/c_counter.h"

#include "examples/xplatform/c_from_swift/counter.h"

using ::swiftexample::Counter;

counter_t counter_create() {
  Counter *counter = new Counter();
  return static_cast<counter_t>(counter);
}

void counter_release(counter_t c) {
  Counter *counter = static_cast<Counter *>(c);
  delete counter;
}

int counter_get(counter_t c) {
  Counter *counter = static_cast<Counter *>(c);
  return counter->Get();
}

void counter_increment(counter_t c) {
  Counter *counter = static_cast<Counter *>(c);
  counter->Increment();
}
