// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <jni.h>
/* Header for class com_example_NativeRunfileFuzzTest */

#ifndef EXAMPLES_JAVA_COM_EXAMPLE_NATIVERUNFILEFUZZTEST_H_
#define EXAMPLES_JAVA_COM_EXAMPLE_NATIVERUNFILEFUZZTEST_H_
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_example_NativeRunfileFuzzTest
 * Method:    loadCppRunfile
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_com_example_NativeRunfileFuzzTest_loadCppRunfile(JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif  // EXAMPLES_JAVA_COM_EXAMPLE_NATIVERUNFILEFUZZTEST_H_
