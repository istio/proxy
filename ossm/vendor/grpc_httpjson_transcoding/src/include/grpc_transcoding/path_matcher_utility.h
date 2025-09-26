/* Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef GRPC_HTTPJSON_TRANSCODING_PATH_MATCHER_UTILITY_H
#define GRPC_HTTPJSON_TRANSCODING_PATH_MATCHER_UTILITY_H

#include "google/api/http.pb.h"
#include "path_matcher.h"

namespace google {
namespace grpc {
namespace transcoding {

class PathMatcherUtility {
 public:
  template <class Method>
  static bool RegisterByHttpRule(
      PathMatcherBuilder<Method> &pmb, const google::api::HttpRule &http_rule,
      const std::unordered_set<std::string> &system_query_parameter_names,
      const Method &method);

  template <class Method>
  static bool RegisterByHttpRule(PathMatcherBuilder<Method> &pmb,
                                 const google::api::HttpRule &http_rule,
                                 const Method &method) {
    return RegisterByHttpRule(pmb, http_rule, std::unordered_set<std::string>(),
                              method);
  }
};

template <class Method>
bool PathMatcherUtility::RegisterByHttpRule(
    PathMatcherBuilder<Method> &pmb, const google::api::HttpRule &http_rule,
    const std::unordered_set<std::string> &system_query_parameter_names,
    const Method &method) {
  bool ok = true;
  switch (http_rule.pattern_case()) {
    case ::google::api::HttpRule::kGet:
      ok = pmb.Register("GET", http_rule.get(), http_rule.body(),
                        system_query_parameter_names, method);
      break;
    case ::google::api::HttpRule::kPut:
      ok = pmb.Register("PUT", http_rule.put(), http_rule.body(),
                        system_query_parameter_names, method);
      break;
    case ::google::api::HttpRule::kPost:
      ok = pmb.Register("POST", http_rule.post(), http_rule.body(),
                        system_query_parameter_names, method);
      break;
    case ::google::api::HttpRule::kDelete:
      ok = pmb.Register("DELETE", http_rule.delete_(), http_rule.body(),
                        system_query_parameter_names, method);
      break;
    case ::google::api::HttpRule::kPatch:
      ok = pmb.Register("PATCH", http_rule.patch(), http_rule.body(),
                        system_query_parameter_names, method);
      break;
    case ::google::api::HttpRule::kCustom:
      ok = pmb.Register(http_rule.custom().kind(), http_rule.custom().path(),
                        http_rule.body(), system_query_parameter_names, method);
      break;
    default:  // ::google::api::HttpRule::PATTEN_NOT_SET
      break;
  }

  for (const auto &additional_binding : http_rule.additional_bindings()) {
    if (!ok) {
      return ok;
    }
    ok = RegisterByHttpRule(pmb, additional_binding,
                            system_query_parameter_names, method);
  }

  return ok;
}

}  // namespace transcoding
}  // namespace grpc
}  // namespace google

#endif  // GRPC_HTTPJSON_TRANSCODING_PATH_MATCHER_UTILITY_H
