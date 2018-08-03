/* Copyright 2018 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ISTIO_UTILS_FILTER_NAMES_H
#define ISTIO_UTILS_FILTER_NAMES_H

#include <string>

namespace istio {
namespace utils {

// These are name of filters that currently output data to dynamicMetadata (by
// convention, under the the entry using filter name itself as key). Define them
// here for easy access.
struct FilterName {
  static const char kJwt[];
  static const char kAuthentication[];
};

}  // namespace utils
}  // namespace istio

#endif  // ISTIO_UTILS_FILTER_NAMES_H
