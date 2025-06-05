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

#if __cplusplus
extern "C" {
#endif

// An opaque type that represents a pointer to the C++ Counter type in C APIs.
typedef void *counter_t;

// C functions that correspond to the C++ interface.
counter_t counter_create();
void counter_release(counter_t c);
int counter_get(counter_t c);
void counter_increment(counter_t c);

#if __cplusplus
}
#endif
