/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#ifndef ISTIO_API_SPEC_PATH_MATCHER_H_
#define ISTIO_API_SPEC_PATH_MATCHER_H_

#include <cstddef>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

#include "src/istio/api_spec/http_template.h"
#include "src/istio/api_spec/path_matcher_node.h"

namespace istio {
namespace api_spec {

template <class Method>
class PathMatcherBuilder;  // required for PathMatcher constructor

// The immutable, thread safe PathMatcher stores a mapping from a combination of
// a service (host) name and a HTTP path to your method (MethodInfo*). It is
// constructed with a PathMatcherBuilder and supports one operation: Lookup.
// Clients may use this method to locate your method (MethodInfo*) for a
// combination of service name and HTTP URL path.
//
// Usage example:
// 1) building the PathMatcher:
//     PathMatcherBuilder builder(false);
//     for each (service_name, http_method, url_path, associated method)
//         builder.register(service_name, http_method, url_path, data);
//     PathMater matcher = builder.Build();
// 2) lookup:
//      MethodInfo * method = matcher.Lookup(service_name, http_method,
//                                           url_path);
//      if (method == nullptr)  failed to find it.
//
template <class Method>
class PathMatcher {
 public:
  ~PathMatcher(){};

  Method Lookup(const std::string& http_method, const std::string& path) const;

 private:
  // Creates a Path Matcher with a Builder by moving the builder's root node.
  explicit PathMatcher(PathMatcherBuilder<Method>&& builder);

  // A root node shared by all services, i.e. paths of all services will be
  // registered to this node.
  std::unique_ptr<PathMatcherNode> root_ptr_;
  // Holds the set of custom verbs found in configured templates.
  std::set<std::string> custom_verbs_;
  // Data we store per each registered method
  struct MethodData {
    Method method;
    std::vector<HttpTemplate::Variable> variables;
    std::string body_field_path;
  };
  // The info associated with each method. The path matcher nodes
  // will hold pointers to MethodData objects in this vector.
  std::vector<std::unique_ptr<MethodData>> methods_;

 private:
  friend class PathMatcherBuilder<Method>;
};

template <class Method>
using PathMatcherPtr = std::unique_ptr<PathMatcher<Method>>;

// This PathMatcherBuilder is used to register path-WrapperGraph pairs and
// instantiate an immutable, thread safe PathMatcher.
//
// The PathMatcherBuilder itself is NOT THREAD SAFE.
template <class Method>
class PathMatcherBuilder {
 public:
  PathMatcherBuilder();
  ~PathMatcherBuilder() {}

  // Registers a method.
  //
  // Registrations are one-to-one. If this function is called more than once, it
  // replaces the existing method. Only the last registered method is stored.
  // Return false if path is an invalid http template.
  bool Register(std::string http_method, std::string path,
                std::string body_field_path, Method method);

  // Returns a unique_ptr to a thread safe PathMatcher that contains all
  // registered path-WrapperGraph pairs. Note the PathMatchBuilder instance
  // will be moved so cannot use after invoking Build().
  PathMatcherPtr<Method> Build();

 private:
  // A root node shared by all services, i.e. paths of all services will be
  // registered to this node.
  std::unique_ptr<PathMatcherNode> root_ptr_;
  // The set of custom verbs configured.
  // TODO: Perhaps this should not be at this level because there will
  // be multiple templates in different services on a server. Consider moving
  // this to PathMatcherNode.
  std::set<std::string> custom_verbs_;
  typedef typename PathMatcher<Method>::MethodData MethodData;
  std::vector<std::unique_ptr<MethodData>> methods_;

  friend class PathMatcher<Method>;
};

namespace {

std::vector<std::string>& split(const std::string& s, char delim,
                                std::vector<std::string>& elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

// Converts a request path into a format that can be used to perform a request
// lookup in the PathMatcher trie. This utility method sanitizes the request
// path and then splits the path into slash separated parts. Returns an empty
// vector if the sanitized path is "/".
//
// custom_verbs is a set of configured custom verbs that are used to match
// against any custom verbs in request path. If the request_path contains a
// custom verb not found in custom_verbs, it is treated as a part of the path.
//
// - Strips off query string: "/a?foo=bar" --> "/a"
// - Collapses extra slashes: "///" --> "/"
std::vector<std::string> ExtractRequestParts(
    std::string path, const std::set<std::string>& custom_verbs) {
  // Remove query parameters.
  path = path.substr(0, path.find_first_of('?'));

  // Replace last ':' with '/' to handle custom verb.
  // But not for /foo:bar/const.
  std::size_t last_colon_pos = path.find_last_of(':');
  std::size_t last_slash_pos = path.find_last_of('/');
  if (last_colon_pos != std::string::npos && last_colon_pos > last_slash_pos) {
    std::string verb = path.substr(last_colon_pos + 1);
    // only verb in the configured custom verbs, treat it as verb
    // replace ":" with / as a separate segment.
    if (custom_verbs.find(verb) != custom_verbs.end()) {
      path[last_colon_pos] = '/';
    }
  }

  std::vector<std::string> result;
  if (path.size() > 0) {
    split(path.substr(1), '/', result);
  }
  // Removes all trailing empty parts caused by extra "/".
  while (!result.empty() && (*(--result.end())).empty()) {
    result.pop_back();
  }
  return result;
}

// Looks up on a PathMatcherNode.
PathMatcherLookupResult LookupInPathMatcherNode(
    const PathMatcherNode& root, const std::vector<std::string>& parts,
    const HttpMethod& http_method) {
  PathMatcherLookupResult result;
  root.LookupPath(parts.begin(), parts.end(), http_method, &result);
  return result;
}

PathMatcherNode::PathInfo TransformHttpTemplate(const HttpTemplate& ht) {
  PathMatcherNode::PathInfo::Builder builder;

  for (const std::string& part : ht.segments()) {
    builder.AppendLiteralNode(part);
  }
  if (!ht.verb().empty()) {
    builder.AppendLiteralNode(ht.verb());
  }

  return builder.Build();
}

}  // namespace

template <class Method>
PathMatcher<Method>::PathMatcher(PathMatcherBuilder<Method>&& builder)
    : root_ptr_(std::move(builder.root_ptr_)),
      custom_verbs_(std::move(builder.custom_verbs_)),
      methods_(std::move(builder.methods_)) {}

// TODO: refactor common code with method above
template <class Method>
Method PathMatcher<Method>::Lookup(const std::string& http_method,
                                   const std::string& path) const {
  const std::vector<std::string> parts =
      ExtractRequestParts(path, custom_verbs_);

  // If service_name has not been registered to ESP and strict_service_matching_
  // is set to false, tries to lookup the method in all registered services.
  if (root_ptr_ == nullptr) {
    return nullptr;
  }

  PathMatcherLookupResult lookup_result =
      LookupInPathMatcherNode(*root_ptr_, parts, http_method);
  // Return nullptr if nothing is found.
  // Not need to check duplication. Only first item is stored for duplicated
  if (lookup_result.data == nullptr) {
    return nullptr;
  }
  MethodData* method_data = reinterpret_cast<MethodData*>(lookup_result.data);
  return method_data->method;
}

// Initializes the builder with a root Path Segment
template <class Method>
PathMatcherBuilder<Method>::PathMatcherBuilder()
    : root_ptr_(new PathMatcherNode()) {}

template <class Method>
PathMatcherPtr<Method> PathMatcherBuilder<Method>::Build() {
  return PathMatcherPtr<Method>(new PathMatcher<Method>(std::move(*this)));
}

// This wrapper converts the |http_rule| into a HttpTemplate. Then, inserts the
// template into the trie.
template <class Method>
bool PathMatcherBuilder<Method>::Register(std::string http_method,
                                          std::string http_template,
                                          std::string body_field_path,
                                          Method method) {
  std::unique_ptr<HttpTemplate> ht = HttpTemplate::Parse(http_template);
  if (nullptr == ht) {
    return false;
  }
  PathMatcherNode::PathInfo path_info = TransformHttpTemplate(*ht);
  if (path_info.path_info().size() == 0) {
    return false;
  }
  // Create & initialize a MethodData struct. Then insert its pointer
  // into the path matcher trie.
  auto method_data = std::unique_ptr<MethodData>(new MethodData());
  method_data->method = method;
  method_data->variables = std::move(ht->Variables());
  method_data->body_field_path = std::move(body_field_path);

  if (!root_ptr_->InsertPath(path_info, http_method, method_data.get(), true)) {
    return false;
  }
  // Add the method_data to the methods_ vector for cleanup
  methods_.emplace_back(std::move(method_data));
  if (!ht->verb().empty()) {
    custom_verbs_.insert(ht->verb());
  }
  return true;
}

}  // namespace api_spec
}  // namespace istio

#endif  // ISTIO_API_SPEC_PATH_MATCHER_H_
