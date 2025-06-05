/*
 * Copyright 2016-2019 Envoy Project Authors
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Create an expression using a foreign function call.
inline WasmResult createExpression(std::string_view expr, uint32_t *token) {
  std::string function = "expr_create";
  char *out = nullptr;
  size_t out_size = 0;
  auto result = proxy_call_foreign_function(function.data(), function.size(), expr.data(),
                                            expr.size(), &out, &out_size);
  if (result == WasmResult::Ok && out_size == sizeof(uint32_t)) {
    *token = *reinterpret_cast<uint32_t *>(out);
  }
  ::free(out);
  return result;
}

// Evaluate an expression using an expression token.
inline std::optional<WasmDataPtr> exprEvaluate(uint32_t token) {
  std::string function = "expr_evaluate";
  char *out = nullptr;
  size_t out_size = 0;
  auto result = proxy_call_foreign_function(function.data(), function.size(),
                                            reinterpret_cast<const char *>(&token),
                                            sizeof(uint32_t), &out, &out_size);
  if (result != WasmResult::Ok) {
    return {};
  }
  return std::make_unique<WasmData>(out, out_size);
}

// Delete an expression using an expression token.
inline WasmResult exprDelete(uint32_t token) {
  std::string function = "expr_delete";
  char *out = nullptr;
  size_t out_size = 0;
  auto result = proxy_call_foreign_function(function.data(), function.size(),
                                            reinterpret_cast<const char *>(&token),
                                            sizeof(uint32_t), &out, &out_size);
  ::free(out);
  return result;
}

template <typename T> inline bool evaluateExpression(uint32_t token, T *out) {
  auto buf = exprEvaluate(token);
  if (!buf.has_value() || buf.value()->size() != sizeof(T)) {
    return false;
  }
  *out = *reinterpret_cast<const T *>(buf.value()->data());
  return true;
}

template <> inline bool evaluateExpression<std::string>(uint32_t token, std::string *out) {
  auto buf = exprEvaluate(token);
  if (!buf.has_value()) {
    return false;
  }
  out->assign(buf.value()->data(), buf.value()->size());
  return true;
}

// Specialization for message types (including struct value for lists and maps)
template <typename T> inline bool evaluateMessage(uint32_t token, T *value_ptr) {
  auto buf = exprEvaluate(token);
  if (!buf.has_value()) {
    return false;
  }
  if (buf.value()->size() == 0) {
    // evaluates to null
    return true;
  }
  return value_ptr->ParseFromArray(buf.value()->data(), buf.value()->size());
}
