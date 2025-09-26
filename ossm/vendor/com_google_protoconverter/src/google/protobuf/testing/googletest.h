/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_PROTOBUF_GOOGLETEST_H__
#define GOOGLE_PROTOBUF_GOOGLETEST_H__

#include <gmock/gmock.h>

#include <string>
#include <vector>

namespace google {
namespace protobuf {

// When running unittests, get the directory containing the source code.
std::string TestSourceDir();

// When running unittests, get a directory where temporary files may be
// placed.
std::string TestTempDir();

// Capture all text written to stdout or stderr.
void CaptureTestStdout();
void CaptureTestStderr();

// Stop capturing stdout or stderr and return the text captured.
std::string GetCapturedTestStdout();
std::string GetCapturedTestStderr();

}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_GOOGLETEST_H__
