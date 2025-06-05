/* Copyright 2016 Google Inc. All Rights Reserved.
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
#ifndef GRPC_TRANSCODING_PATH_MATCHER_H_
#define GRPC_TRANSCODING_PATH_MATCHER_H_

#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "absl/strings/str_split.h"
#include "http_template.h"
#include "path_matcher_node.h"
#include "percent_encoding.h"

namespace google {
namespace grpc {
namespace transcoding {

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

  Method Lookup(const std::string& http_method, const std::string& path,
                const std::string& query_params,
                std::vector<VariableBinding>* variable_bindings,
                std::string* body_field_path) const;

  Method Lookup(const std::string& http_method, const std::string& path) const;

 private:
  // Creates a Path Matcher with a Builder by moving the builder's root node.
  explicit PathMatcher(PathMatcherBuilder<Method>&& builder);

  // A root node shared by all services, i.e. paths of all services will be
  // registered to this node.
  std::unique_ptr<PathMatcherNode> root_ptr_;
  // Holds the set of custom verbs found in configured templates.
  std::unordered_set<std::string> custom_verbs_;
  // Data we store per each registered method
  struct MethodData {
    Method method;
    std::vector<HttpTemplate::Variable> variables;
    std::string body_field_path;
    std::unordered_set<std::string> system_query_parameter_names;
  };
  // The info associated with each method. The path matcher nodes
  // will hold pointers to MethodData objects in this vector.
  std::vector<std::unique_ptr<MethodData>> methods_;
  UrlUnescapeSpec path_unescape_spec_;
  bool query_param_unescape_plus_;
  bool match_unregistered_custom_verb_;

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
  bool Register(
      const std::string& http_method, const std::string& path,
      const std::string& body_field_path,
      const std::unordered_set<std::string>& system_query_parameter_names,
      Method method);
  bool Register(const std::string& http_method, const std::string& path,
                const std::string& body_field_path, Method method);

  // Change unescaping behavior, see UrlUnescapeSpec for available options.
  // This only applies to path, not query parameters.
  void SetUrlUnescapeSpec(UrlUnescapeSpec path_unescape_spec) {
    path_unescape_spec_ = path_unescape_spec;
  }
  // If true, unescape '+' in query parameters to space. Default is false.
  // To support [HTML 2.0 or
  // RFC1866](https://tools.ietf.org/html/rfc1866#section-8.2.1).
  void SetQueryParamUnescapePlus(bool query_param_unescape_plus) {
    query_param_unescape_plus_ = query_param_unescape_plus;
  }

  // If true, try to match the custom verb even if it is unregistered. By
  // default, only match when it is registered.
  void SetMatchUnregisteredCustomVerb(bool match_unregistered_custom_verb) {
    match_unregistered_custom_verb_ = match_unregistered_custom_verb;
  }

  // If true, all further calls to Register will return false on duplicate
  // paths in addition to invalid ones. Default is false.
  void SetFailRegistrationOnDuplicate(bool fail_registration_on_duplicate) {
    fail_registration_on_duplicate_ = fail_registration_on_duplicate;
  }

  // Returns a unique_ptr to a thread safe PathMatcher that contains all
  // registered path-WrapperGraph pairs. Note the PathMatchBuilder instance
  // will be moved so cannot use after invoking Build().
  PathMatcherPtr<Method> Build();

 private:
  // Inserts a path to a PathMatcherNode.
  bool InsertPathToNode(const PathMatcherNode::PathInfo& path,
                        void* method_data, std::string http_method,
                        PathMatcherNode* root_ptr);
  // A root node shared by all services, i.e. paths of all services will be
  // registered to this node.
  std::unique_ptr<PathMatcherNode> root_ptr_;
  // The set of custom verbs configured.
  // TODO: Perhaps this should not be at this level because there will
  // be multiple templates in different services on a server. Consider moving
  // this to PathMatcherNode.
  std::unordered_set<std::string> custom_verbs_;
  typedef typename PathMatcher<Method>::MethodData MethodData;
  std::vector<std::unique_ptr<MethodData>> methods_;
  UrlUnescapeSpec path_unescape_spec_ =
      UrlUnescapeSpec::kAllCharactersExceptReserved;
  bool query_param_unescape_plus_ = false;
  bool match_unregistered_custom_verb_ = false;
  bool fail_registration_on_duplicate_ = false;

  friend class PathMatcher<Method>;
};

namespace {

void ExtractBindingsFromPath(const std::vector<HttpTemplate::Variable>& vars,
                             const std::vector<std::string>& parts,
                             UrlUnescapeSpec unescape_spec,
                             std::vector<VariableBinding>* bindings) {
  for (const auto& var : vars) {
    // Determine the subpath bound to the variable based on the
    // [start_segment, end_segment) segment range of the variable.
    //
    // In case of matching "**" - end_segment is negative and is relative to
    // the end such that end_segment = -1 will match all subsequent segments.
    VariableBinding binding;
    binding.field_path = var.field_path;
    // Calculate the absolute index of the ending segment in case it's negative.
    size_t end_segment = (var.end_segment >= 0)
                             ? var.end_segment
                             : parts.size() + var.end_segment + 1;
    // It is multi-part match if we have more than one segment. We also make
    // sure that a single URL segment match with ** is also considered a
    // multi-part match by checking if it->second.end_segment is negative.
    bool is_multipart =
        (end_segment - var.start_segment) > 1 || var.end_segment < 0;
    const UrlUnescapeSpec var_unescape_spec =
        is_multipart ? unescape_spec : UrlUnescapeSpec::kAllCharacters;

    // Joins parts with "/"  to form a path string.
    for (size_t i = var.start_segment; i < end_segment; ++i) {
      // For multipart matches only unescape non-reserved characters.
      binding.value += UrlUnescapeString(parts[i], var_unescape_spec, false);
      if (i < end_segment - 1) {
        binding.value += "/";
      }
    }
    bindings->emplace_back(binding);
  }
}

void ExtractBindingsFromQueryParameters(
    const std::string& query_params,
    const std::unordered_set<std::string>& system_params,
    bool query_param_unescape_plus, std::vector<VariableBinding>* bindings) {
  // The bindings in URL the query parameters have the following form:
  //      <field_path1>=value1&<field_path2>=value2&...&<field_pathN>=valueN
  // Query parameters may also contain system parameters such as `api_key`.
  // We'll need to ignore these. Example:
  //      book.id=123&book.author=Neal%20Stephenson&api_key=AIzaSyAz7fhBkC35D2M
  std::vector<std::string> params = absl::StrSplit(query_params, '&');
  for (const auto& param : params) {
    size_t pos = param.find('=');
    if (pos != 0 && pos != std::string::npos) {
      auto name = param.substr(0, pos);
      // Make sure the query parameter is not a system parameter (e.g.
      // `api_key`) before adding the binding.
      if (system_params.find(name) == std::end(system_params)) {
        // The name of the parameter is a field path, which is a dot-delimited
        // sequence of field names that identify the (potentially deep) field
        // in the request, e.g. `book.author.name`.
        VariableBinding binding;
        binding.field_path = absl::StrSplit(name, '.');
        binding.value = UrlUnescapeString(param.substr(pos + 1),
                                          UrlUnescapeSpec::kAllCharacters,
                                          query_param_unescape_plus);
        bindings->emplace_back(std::move(binding));
      }
    }
  }
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
    std::string path, const std::unordered_set<std::string>& custom_verbs,
    std::string& verb, bool match_unregistered_custom_verb) {
  // Remove query parameters.
  path = path.substr(0, path.find_first_of('?'));

  // Replace last ':' with '/' to handle custom verb.
  // But not for /foo:bar/const.
  std::size_t last_colon_pos = path.find_last_of(':');
  std::size_t last_slash_pos = path.find_last_of('/');
  if (last_colon_pos != std::string::npos && last_colon_pos > last_slash_pos) {
    std::string tmp_verb = path.substr(last_colon_pos + 1);
    // Only when chek_unregistered_custom_verb=true or the verb is in the
    // configured custom verbs, treat it as verb
    if (match_unregistered_custom_verb ||
        custom_verbs.find(tmp_verb) != custom_verbs.end()) {
      verb = tmp_verb;
      path = path.substr(0, last_colon_pos);
    }
  }

  std::vector<std::string> result;
  if (path.size() > 0) {
    result = absl::StrSplit(path.substr(1), '/');
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

  return builder.Build();
}

}  // namespace

template <class Method>
PathMatcher<Method>::PathMatcher(PathMatcherBuilder<Method>&& builder)
    : root_ptr_(std::move(builder.root_ptr_)),
      custom_verbs_(std::move(builder.custom_verbs_)),
      methods_(std::move(builder.methods_)),
      path_unescape_spec_(builder.path_unescape_spec_),
      query_param_unescape_plus_(builder.query_param_unescape_plus_),
      match_unregistered_custom_verb_(builder.match_unregistered_custom_verb_) {
}

// Lookup is a wrapper method for the recursive node Lookup. First, the wrapper
// splits the request path into slash-separated path parts. Next, the method
// checks that the |http_method| is supported. If not, then it returns an empty
// WrapperGraph::SharedPtr. Next, this method invokes the node's Lookup on
// the extracted |parts|. Finally, it fills the mapping from variables to their
// values parsed from the path.
// TODO: cache results by adding get/put methods here (if profiling reveals
// benefit)
template <class Method>
Method PathMatcher<Method>::Lookup(
    const std::string& http_method, const std::string& path,
    const std::string& query_params,
    std::vector<VariableBinding>* variable_bindings,
    std::string* body_field_path) const {
  std::string verb;
  const std::vector<std::string> parts = ExtractRequestParts(
      path, custom_verbs_, verb, match_unregistered_custom_verb_);

  // If service_name has not been registered to ESP and strict_service_matching_
  // is set to false, tries to lookup the method in all registered services.
  if (root_ptr_ == nullptr) {
    return nullptr;
  }

  PathMatcherLookupResult lookup_result =
      LookupInPathMatcherNode(*root_ptr_, parts, http_method + verb);
  // Return nullptr if nothing is found or the result is marked for duplication.
  if (lookup_result.data == nullptr || lookup_result.is_multiple) {
    return nullptr;
  }
  MethodData* method_data = reinterpret_cast<MethodData*>(lookup_result.data);
  if (variable_bindings != nullptr) {
    variable_bindings->clear();
    ExtractBindingsFromPath(method_data->variables, parts, path_unescape_spec_,
                            variable_bindings);
    ExtractBindingsFromQueryParameters(
        query_params, method_data->system_query_parameter_names,
        query_param_unescape_plus_, variable_bindings);
  }
  if (body_field_path != nullptr) {
    *body_field_path = method_data->body_field_path;
  }
  return method_data->method;
}

// TODO: refactor common code with method above
template <class Method>
Method PathMatcher<Method>::Lookup(const std::string& http_method,
                                   const std::string& path) const {
  std::string verb;
  const std::vector<std::string> parts = ExtractRequestParts(
      path, custom_verbs_, verb, match_unregistered_custom_verb_);

  // If service_name has not been registered to ESP and strict_service_matching_
  // is set to false, tries to lookup the method in all registered services.
  if (root_ptr_ == nullptr) {
    return nullptr;
  }

  PathMatcherLookupResult lookup_result =
      LookupInPathMatcherNode(*root_ptr_, parts, http_method + verb);
  // Return nullptr if nothing is found or the result is marked for duplication.
  if (lookup_result.data == nullptr || lookup_result.is_multiple) {
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

template <class Method>
bool PathMatcherBuilder<Method>::InsertPathToNode(
    const PathMatcherNode::PathInfo& path, void* method_data,
    std::string http_method, PathMatcherNode* root_ptr) {
  if (!root_ptr->InsertPath(path, http_method, method_data, true)) {
    if (fail_registration_on_duplicate_) {
      return false;
    }
  }
  return true;
}

// This wrapper converts the |http_rule| into a HttpTemplate. Then, inserts the
// template into the trie.
template <class Method>
bool PathMatcherBuilder<Method>::Register(
    const std::string& http_method, const std::string& http_template,
    const std::string& body_field_path,
    const std::unordered_set<std::string>& system_query_parameter_names,
    Method method) {
  std::unique_ptr<HttpTemplate> ht(HttpTemplate::Parse(http_template));
  if (nullptr == ht) {
    return false;
  }
  PathMatcherNode::PathInfo path_info = TransformHttpTemplate(*ht);

  // Create & initialize a MethodData struct. Then insert its pointer
  // into the path matcher trie.
  auto method_data = std::unique_ptr<MethodData>(new MethodData());
  method_data->method = method;
  method_data->variables = std::move(ht->Variables());
  method_data->body_field_path = body_field_path;
  method_data->system_query_parameter_names = system_query_parameter_names;

  if (!InsertPathToNode(path_info, method_data.get(), http_method + ht->verb(),
                        root_ptr_.get())) {
    return false;
  }
  // Add the method_data to the methods_ vector for cleanup
  methods_.emplace_back(std::move(method_data));
  if (!ht->verb().empty()) {
    custom_verbs_.insert(ht->verb());
  }
  return true;
}

template <class Method>
bool PathMatcherBuilder<Method>::Register(const std::string& http_method,
                                          const std::string& http_template,
                                          const std::string& body_field_path,
                                          Method method) {
  return Register(http_method, http_template, body_field_path,
                  std::unordered_set<std::string>(), method);
}

}  // namespace transcoding
}  // namespace grpc
}  // namespace google

#endif  // GRPC_TRANSCODING_PATH_MATCHER_H_
