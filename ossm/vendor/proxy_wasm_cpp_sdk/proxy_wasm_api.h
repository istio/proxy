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

/*
 * Intrinsic high-level support functions available to WASM modules.
 */
// NOLINT(namespace-envoy)
#pragma once

#ifdef PROXY_WASM_PROTOBUF
#include "google/protobuf/message_lite.h"
#endif

#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#define CHECK_RESULT(_c)                                                                           \
  do {                                                                                             \
    if ((_c) != WasmResult::Ok) {                                                                  \
      proxy_log(LogLevel::critical, #_c, sizeof(#_c) - 1);                                         \
      abort();                                                                                     \
    }                                                                                              \
  } while (0)

//
// High Level C++ API.
//
class ContextBase;
class RootContext;
class Context;

// Note: exceptions are currently not supported.
#define WASM_EXCEPTIONS 0
#if WASM_EXCEPTIONS
class ProxyException : std::runtime_error {
public:
  ProxyException(const std::string &message) : std::runtime_error(message) {}
};
#endif

inline WasmResult logTrace(std::string_view logMessage) {
  return proxy_log(LogLevel::trace, logMessage.data(), logMessage.size());
}
inline WasmResult logDebug(std::string_view logMessage) {
  return proxy_log(LogLevel::debug, logMessage.data(), logMessage.size());
}
inline WasmResult logInfo(std::string_view logMessage) {
  return proxy_log(LogLevel::info, logMessage.data(), logMessage.size());
}
inline WasmResult logWarn(std::string_view logMessage) {
  return proxy_log(LogLevel::warn, logMessage.data(), logMessage.size());
}
inline WasmResult logError(std::string_view logMessage) {
  return proxy_log(LogLevel::error, logMessage.data(), logMessage.size());
}
inline WasmResult logCritical(std::string_view logMessage) {
  return proxy_log(LogLevel::critical, logMessage.data(), logMessage.size());
}
inline void logAbort(std::string_view logMessag) {
  logCritical(logMessag);
  abort();
}

#define LOG(_level, ...)                                                                           \
  log##_level(std::string("[") + __FILE__ + ":" + std::to_string(__LINE__) +                       \
              "]::" + __FUNCTION__ + "() " + __VA_ARGS__)
#define LOG_TRACE(...) LOG(Trace, __VA_ARGS__)
#define LOG_DEBUG(...) LOG(Debug, __VA_ARGS__)
#define LOG_INFO(...) LOG(Info, __VA_ARGS__)
#define LOG_WARN(...) LOG(Warn, __VA_ARGS__)
#define LOG_ERROR(...) LOG(Error, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG(Critical, __VA_ARGS__)

// Buffers coming into the WASM filter.
class WasmData {
public:
  WasmData(const char *data, size_t size) : data_(data), size_(size) {}
  ~WasmData() { ::free(const_cast<char *>(data_)); }
  const char *data() { return data_; }
  size_t size() { return size_; }
  std::string_view view() { return {data_, size_}; }
  std::string toString() { return std::string(view()); }
  std::vector<std::pair<std::string_view, std::string_view>> pairs();
  template <typename T> T proto() {
    T p;
    p.ParseFromArray(data_, size_);
    return p;
  }

  WasmData &operator=(const WasmData &) = delete;
  WasmData(const WasmData &) = delete;

private:
  const char *data_;
  size_t size_;
};
typedef std::unique_ptr<WasmData> WasmDataPtr;

inline std::vector<std::pair<std::string_view, std::string_view>> WasmData::pairs() {
  std::vector<std::pair<std::string_view, std::string_view>> result;
  if (!data())
    return result;
  auto p = data();
  int n = *reinterpret_cast<const int *>(p);
  p += sizeof(int);
  result.resize(n);
  auto s = p + n * 8;
  for (int i = 0; i < n; i++) {
    int size = *reinterpret_cast<const int *>(p);
    p += sizeof(int);
    result[i].first = std::string_view(s, size);
    s += size + 1;
    size = *reinterpret_cast<const int *>(p);
    p += sizeof(int);
    result[i].second = std::string_view(s, size);
    s += size + 1;
  }
  return result;
}

template <typename Pairs> size_t pairsSize(const Pairs &result) {
  size_t size = 4; // number of headers
  for (auto &p : result) {
    size += 8;                   // size of key, size of value
    size += p.first.size() + 1;  // null terminated key
    size += p.second.size() + 1; // null terminated value
  }
  return size;
}

template <typename Pairs> void marshalPairs(const Pairs &result, char *buffer) {
  char *b = buffer;
  *reinterpret_cast<uint32_t *>(b) = result.size();
  b += sizeof(uint32_t);
  for (auto &p : result) {
    *reinterpret_cast<uint32_t *>(b) = p.first.size();
    b += sizeof(uint32_t);
    *reinterpret_cast<uint32_t *>(b) = p.second.size();
    b += sizeof(uint32_t);
  }
  for (auto &p : result) {
    memcpy(b, p.first.data(), p.first.size());
    b += p.first.size();
    *b++ = 0;
    memcpy(b, p.second.data(), p.second.size());
    b += p.second.size();
    *b++ = 0;
  }
}

template <typename Pairs> void exportPairs(const Pairs &pairs, const char **ptr, size_t *size_ptr) {
  if (pairs.empty()) {
    *ptr = nullptr;
    *size_ptr = 0;
    return;
  }
  size_t size = pairsSize(pairs);
  char *buffer = static_cast<char *>(::malloc(size));
  marshalPairs(pairs, buffer);
  *size_ptr = size;
  *ptr = buffer;
}

struct PairHash {
  template <typename T, typename U> std::size_t operator()(const std::pair<T, U> &x) const {
    return std::hash<T>()(x.first) + std::hash<U>()(x.second);
  }
};

struct Tuple3Hash {
  template <typename T, typename U, typename V>
  std::size_t operator()(const std::tuple<T, U, V> &x) const {
    return std::hash<T>()(std::get<0>(x)) + std::hash<U>()(std::get<1>(x)) +
           std::hash<V>()(std::get<2>(x));
  }
};

using HeaderStringPairs = std::vector<std::pair<std::string, std::string>>;

class GrpcCallHandlerBase {
public:
  GrpcCallHandlerBase() {}
  virtual ~GrpcCallHandlerBase() {}

  RootContext *context() { return context_; }

  void cancel();
  uint32_t token() { return token_; }

  virtual void onSuccess(size_t body_size) = 0;
  virtual void onFailure(GrpcStatus status) = 0;

private:
  friend class RootContext;

  RootContext *context_{nullptr};
  uint32_t token_;
};

template <typename Message> class GrpcCallHandler : public GrpcCallHandlerBase {
public:
  GrpcCallHandler() : GrpcCallHandlerBase() {}
  virtual ~GrpcCallHandler() {}

  virtual void onSuccess(size_t body_size) = 0;
};

class GrpcStreamHandlerBase {
public:
  GrpcStreamHandlerBase() {}
  virtual ~GrpcStreamHandlerBase() {}

  RootContext *context() { return context_; }

  // NB: with end_of_stream == true, callbacks can still occur: reset() to
  // prevent further callbacks.
  WasmResult send(std::string_view message, bool end_of_stream);
  void close(); // NB: callbacks can still occur: reset() to prevent further
                // callbacks.
  void reset();
  uint32_t token() { return token_; }

  virtual void onReceiveInitialMetadata(uint32_t /* headers */) {}
  virtual void onReceiveTrailingMetadata(uint32_t /* trailers */) {}
  virtual void onReceive(size_t body_size) = 0;
  virtual void onRemoteClose(GrpcStatus status) = 0;

protected:
  friend class RootContext;

  void doRemoteClose(GrpcStatus status);

  bool local_close_{false};
  bool remote_close_{false};
  RootContext *context_{nullptr};
  uint32_t token_;
};

template <typename Request, typename Response>
class GrpcStreamHandler : public GrpcStreamHandlerBase {
public:
  GrpcStreamHandler() : GrpcStreamHandlerBase() {}
  virtual ~GrpcStreamHandler() {}

  WasmResult send(const Request &message, bool end_of_stream) {
    std::string output;
    if (!message.SerializeToString(&output)) {
      return WasmResult::SerializationFailure;
    }
    GrpcStreamHandlerBase::send(output, end_of_stream);
    local_close_ = local_close_ || end_of_stream;
    return WasmResult::Ok;
  }

  virtual void onReceive(size_t body_size) = 0;
};

// Behavior supported by all contexts.
class ContextBase {
public:
  explicit ContextBase(uint32_t id) : id_(id) {}
  virtual ~ContextBase() {}

  uint32_t id() { return id_; }

  // Make this context the effective context for calls out of the VM.
  WasmResult setEffectiveContext();

  virtual RootContext *asRoot() { return nullptr; }
  virtual Context *asContext() { return nullptr; }

  virtual void onCreate() {}
  virtual bool onDoneBase() = 0;
  // Called on Stream Context after onDone when logging is requested or called on Root Context
  // if so requested.
  virtual void onLog() {}
  // Called to indicate that no more calls will come and this context is being
  // deleted.
  virtual void onDelete() {} // Called when the stream or VM is being deleted.
  // Called when a foreign function event arrives.
  virtual void onForeignFunction(uint32_t /* foreign_function_id */, uint32_t /* data_size */) {}

  // Return the log level configured for the "wasm" logger in the host
  WasmResult getLogLevel(LogLevel *level) { return proxy_get_log_level(level); }

  using HttpCallCallback =
      std::function<void(uint32_t, size_t, uint32_t)>; // headers, body_size, trailers
  using GrpcSimpleCallCallback = std::function<void(GrpcStatus status, size_t body_size)>;

private:
  uint32_t id_;
};

// A context unique for each root_id for a use-case (e.g. filter) compiled into
// module.
class RootContext : public ContextBase {
public:
  RootContext(uint32_t id, std::string_view root_id) : ContextBase(id), root_id_(root_id) {}
  ~RootContext() {}

  std::string_view root_id() { return root_id_; }

  RootContext *asRoot() override { return this; }
  Context *asContext() override { return nullptr; }

  // Can be used to validate the configuration (e.g. in the control plane).
  // Returns false if the configuration is invalid.
  virtual bool validateConfiguration(size_t /* configuration_size */) { return true; }
  // Called once when the VM loads and once when each hook loads and whenever
  // configuration changes. Returns false if the configuration is invalid.
  virtual bool onConfigure(size_t /* configuration_size */) { return true; }
  // Called when each hook loads. Returns false if the configuration is
  // invalid.
  virtual bool onStart(size_t /* vm_configuration_size */) { return true; }
  // Called when the timer goes off.
  virtual void onTick() {}
  // Called when data arrives on a SharedQueue.
  virtual void onQueueReady(uint32_t /* token */) {}

  virtual bool onDone() { return true; } // Called when the VM is being torn down.
  void done(); // Report that we are now done following returning false from onDone.

  // Low level HTTP/gRPC interface.
  virtual void onHttpCallResponse(uint32_t token, uint32_t headers, size_t body_size,
                                  uint32_t trailers);
  virtual void onGrpcReceiveInitialMetadata(uint32_t token, uint32_t headers);
  virtual void onGrpcReceiveTrailingMetadata(uint32_t token, uint32_t trailers);
  virtual void onGrpcReceive(uint32_t token, size_t body_size);
  virtual void onGrpcClose(uint32_t token, GrpcStatus status);

  // Default high level HTTP/gRPC interface. NB: overriding the low level
  // interface will disable this interface. Returns false on setup error.
  WasmResult httpCall(std::string_view uri, const HeaderStringPairs &request_headers,
                      std::string_view request_body, const HeaderStringPairs &request_trailers,
                      uint32_t timeout_milliseconds, HttpCallCallback callback);
  // NB: the message is the response if status == OK and an error message
  // otherwise. Returns false on setup error.
  WasmResult grpcSimpleCall(std::string_view service, std::string_view service_name,
                            std::string_view method_name, const HeaderStringPairs &initial_metadata,
                            std::string_view request, uint32_t timeout_milliseconds,
                            GrpcSimpleCallCallback callback);
  WasmResult grpcSimpleCall(std::string_view service, std::string_view service_name,
                            std::string_view method_name, const HeaderStringPairs &initial_metadata,
                            std::string_view request, uint32_t timeout_milliseconds,
                            std::function<void(size_t body_size)> success_callback,
                            std::function<void(GrpcStatus status)> failure_callback) {
    auto callback = [success_callback, failure_callback](GrpcStatus status, size_t body_size) {
      if (status == GrpcStatus::Ok) {
        success_callback(body_size);
      } else {
        failure_callback(status);
      }
    };
    return grpcSimpleCall(service, service_name, method_name, initial_metadata, request,
                          timeout_milliseconds, callback);
  }
  WasmResult grpcCallHandler(std::string_view service, std::string_view service_name,
                             std::string_view method_name,
                             const HeaderStringPairs &initial_metadata, std::string_view request,
                             uint32_t timeout_milliseconds,
                             std::unique_ptr<GrpcCallHandlerBase> handler);
#ifdef PROXY_WASM_PROTOBUF
  WasmResult grpcSimpleCall(std::string_view service, std::string_view service_name,
                            std::string_view method_name, const HeaderStringPairs &initial_metadata,
                            const google::protobuf::MessageLite &request,
                            uint32_t timeout_milliseconds, GrpcSimpleCallCallback callback) {
    std::string serialized_request;
    if (!request.SerializeToString(&serialized_request)) {
      return WasmResult::SerializationFailure;
    }
    return grpcSimpleCall(service, service_name, method_name, initial_metadata, serialized_request,
                          timeout_milliseconds, callback);
  }
  WasmResult grpcSimpleCall(std::string_view service, std::string_view service_name,
                            std::string_view method_name, const HeaderStringPairs &initial_metadata,
                            const google::protobuf::MessageLite &request,
                            uint32_t timeout_milliseconds,
                            std::function<void(size_t body_size)> success_callback,
                            std::function<void(GrpcStatus status)> failure_callback) {
    std::string serialized_request;
    if (!request.SerializeToString(&serialized_request)) {
      return WasmResult::SerializationFailure;
    }
    return grpcSimpleCall(service, service_name, method_name, initial_metadata, serialized_request,
                          timeout_milliseconds, success_callback, failure_callback);
  }
  // Returns false on setup error.
  WasmResult grpcCallHandler(std::string_view service, std::string_view service_name,
                             std::string_view method_name,
                             const HeaderStringPairs &initial_metadata,
                             const google::protobuf::MessageLite &request,
                             uint32_t timeout_milliseconds,
                             std::unique_ptr<GrpcCallHandlerBase> handler) {
    std::string serialized_request;
    if (!request.SerializeToString(&serialized_request)) {
      return WasmResult::SerializationFailure;
    }
    return grpcCallHandler(service, service_name, method_name, initial_metadata, serialized_request,
                           timeout_milliseconds, std::move(handler));
  }
#endif
  // Returns false on setup error.
  WasmResult grpcStreamHandler(std::string_view service, std::string_view service_name,
                               std::string_view method_name,
                               const HeaderStringPairs &initial_metadata,
                               std::unique_ptr<GrpcStreamHandlerBase> handler);

private:
  friend class GrpcCallHandlerBase;
  friend class GrpcStreamHandlerBase;

  bool onDoneBase() override { return onDone(); }

  const std::string root_id_;
  std::unordered_map<uint32_t, HttpCallCallback> http_calls_;
  std::unordered_map<uint32_t, GrpcSimpleCallCallback> simple_grpc_calls_;
  std::unordered_map<uint32_t, std::unique_ptr<GrpcCallHandlerBase>> grpc_calls_;
  std::unordered_map<uint32_t, std::unique_ptr<GrpcStreamHandlerBase>> grpc_streams_;
};

RootContext *getRoot(std::string_view root_id);

// Context for a stream. The distinguished context id == 0 is used for
// non-stream calls.
class Context : public ContextBase {
public:
  Context(uint32_t id, RootContext *root) : ContextBase(id), root_(root) {}
  virtual ~Context() {}

  RootContext *root() { return root_; }

  RootContext *asRoot() override { return nullptr; }
  Context *asContext() override { return this; }

  virtual FilterStatus onNewConnection() { return FilterStatus::Continue; }
  virtual FilterStatus onDownstreamData(size_t, bool) { return FilterStatus::Continue; }
  virtual FilterStatus onUpstreamData(size_t, bool) { return FilterStatus::Continue; }
  virtual void onDownstreamConnectionClose(CloseType) {}
  virtual void onUpstreamConnectionClose(CloseType) {}

  virtual FilterHeadersStatus onRequestHeaders(uint32_t, bool) {
    return FilterHeadersStatus::Continue;
  }
  virtual FilterMetadataStatus onRequestMetadata(uint32_t) {
    return FilterMetadataStatus::Continue;
  }
  virtual FilterDataStatus onRequestBody(size_t /* body_buffer_length */,
                                         bool /* end_of_stream */) {
    return FilterDataStatus::Continue;
  }
  virtual FilterTrailersStatus onRequestTrailers(uint32_t) {
    return FilterTrailersStatus::Continue;
  }
  virtual FilterHeadersStatus onResponseHeaders(uint32_t, bool) {
    return FilterHeadersStatus::Continue;
  }
  virtual FilterMetadataStatus onResponseMetadata(uint32_t) {
    return FilterMetadataStatus::Continue;
  }
  virtual FilterDataStatus onResponseBody(size_t /* body_buffer_length */,
                                          bool /* end_of_stream */) {
    return FilterDataStatus::Continue;
  }
  virtual FilterTrailersStatus onResponseTrailers(uint32_t) {
    return FilterTrailersStatus::Continue;
  }
  virtual void onDone() {} // Called when the stream has completed.

private:
  // For stream Contexts, onDone always returns true.
  bool onDoneBase() override {
    onDone();
    return true;
  }

  RootContext *root_{};
};

// Returns nullptr if the Context no longer exists (i.e. the stream has been
// destroyed).
Context *getContext(uint32_t context_id);
RootContext *getRootContext(uint32_t context_id);
ContextBase *getContextBase(uint32_t context_id);

using RootFactory =
    std::function<std::unique_ptr<RootContext>(uint32_t id, std::string_view root_id)>;
using ContextFactory = std::function<std::unique_ptr<Context>(uint32_t id, RootContext *root)>;

// Create a factory from a class name.
#define ROOT_FACTORY(_c)                                                                           \
  [](uint32_t id, std::string_view root_id) -> std::unique_ptr<RootContext> {                      \
    return std::make_unique<_c>(id, root_id);                                                      \
  }
#define CONTEXT_FACTORY(_c)                                                                        \
  [](uint32_t id, RootContext *root) -> std::unique_ptr<Context> {                                 \
    return std::make_unique<_c>(id, root);                                                         \
  }

// Register Context factory.
// e.g. static RegisterContextFactory
// register_MyContext(CONTEXT_FACTORY(MyContext));
struct RegisterContextFactory {
  RegisterContextFactory(ContextFactory context_factory, RootFactory root_factory,
                         std::string_view root_id = "");
  explicit RegisterContextFactory(RootFactory root_factory, std::string_view root_id = "")
      : RegisterContextFactory(nullptr, root_factory, root_id) {}
  explicit RegisterContextFactory(ContextFactory context_factory, std::string_view root_id = "")
      : RegisterContextFactory(context_factory, nullptr, root_id) {}
};

inline std::pair<uint32_t, WasmDataPtr> getStatus() {
  uint32_t code = 0;
  const char *value_ptr = nullptr;
  size_t value_size = 0;
  CHECK_RESULT(proxy_get_status(&code, &value_ptr, &value_size));
  return std::make_pair(code, std::make_unique<WasmData>(value_ptr, value_size));
}

// Generic selector
inline std::optional<WasmDataPtr>
getProperty(const std::initializer_list<std::string_view> &parts) {
  size_t size = 0;
  for (auto part : parts) {
    size += part.size() + 1; // null terminated string value
  }

  char *buffer = static_cast<char *>(::malloc(size));
  char *b = buffer;

  for (auto part : parts) {
    memcpy(b, part.data(), part.size());
    b += part.size();
    *b++ = 0;
  }

  const char *value_ptr = nullptr;
  size_t value_size = 0;
  auto result = proxy_get_property(buffer, size, &value_ptr, &value_size);
  ::free(buffer);
  if (result != WasmResult::Ok) {
    return {};
  }
  return std::make_unique<WasmData>(value_ptr, value_size);
}

template <typename S> inline std::optional<WasmDataPtr> getProperty(const std::vector<S> &parts) {
  size_t size = 0;
  for (auto part : parts) {
    size += part.size() + 1; // null terminated string value
  }

  char *buffer = static_cast<char *>(::malloc(size));
  char *b = buffer;

  for (auto part : parts) {
    memcpy(b, part.data(), part.size());
    b += part.size();
    *b++ = 0;
  }

  const char *value_ptr = nullptr;
  size_t value_size = 0;
  auto result = proxy_get_property(buffer, size, &value_ptr, &value_size);
  ::free(buffer);
  if (result != WasmResult::Ok) {
    return {};
  }
  return std::make_unique<WasmData>(value_ptr, value_size);
}

// Generic property reader for basic types: int64, uint64, double, bool
// Durations are represented as int64 nanoseconds.
// Timestamps are represented as int64 Unix nanoseconds.
// Strings and bytes are represented as std::string.
template <typename T>
inline bool getValue(const std::initializer_list<std::string_view> &parts, T *out) {
  auto buf = getProperty(parts);
  if (!buf.has_value() || buf.value()->size() != sizeof(T)) {
    return false;
  }
  *out = *reinterpret_cast<const T *>(buf.value()->data());
  return true;
}

// Specialization for bytes and string values
template <>
inline bool getValue<std::string>(const std::initializer_list<std::string_view> &parts,
                                  std::string *out) {
  auto buf = getProperty(parts);
  if (!buf.has_value()) {
    return false;
  }
  out->assign(buf.value()->data(), buf.value()->size());
  return true;
}

template <typename S, typename T> inline bool getValue(const std::vector<S> &parts, T *out) {
  auto buf = getProperty(parts);
  if (!buf.has_value() || buf.value()->size() != sizeof(T)) {
    return false;
  }
  *out = *reinterpret_cast<const T *>(buf.value()->data());
  return true;
}

template <>
inline bool getValue<std::string, std::string>(const std::vector<std::string> &parts,
                                               std::string *out) {
  auto buf = getProperty(parts);
  if (!buf.has_value()) {
    return false;
  }
  out->assign(buf.value()->data(), buf.value()->size());
  return true;
}

template <>
inline bool getValue<std::string_view, std::string>(const std::vector<std::string_view> &parts,
                                                    std::string *out) {
  auto buf = getProperty(parts);
  if (!buf.has_value()) {
    return false;
  }
  out->assign(buf.value()->data(), buf.value()->size());
  return true;
}

// Specialization for message types (including struct value for lists and maps)
template <typename T>
inline bool getMessageValue(const std::initializer_list<std::string_view> &parts, T *value_ptr) {
  auto buf = getProperty(parts);
  if (!buf.has_value()) {
    return false;
  }
  if (buf.value()->size() == 0) {
    value_ptr = nullptr;
    return true;
  }
  return value_ptr->ParseFromArray(buf.value()->data(), buf.value()->size());
}

template <typename S, typename T>
inline bool getMessageValue(const std::vector<S> &parts, T *value_ptr) {
  auto buf = getProperty(parts);
  if (!buf.has_value()) {
    return false;
  }
  if (buf.value()->size() == 0) {
    value_ptr = nullptr;
    return true;
  }
  return value_ptr->ParseFromArray(buf.value()->data(), buf.value()->size());
}

inline WasmResult setFilterState(std::string_view key, std::string_view value) {
  return static_cast<WasmResult>(
      proxy_set_property(key.data(), key.size(), value.data(), value.size()));
}

inline WasmResult setFilterStateStringValue(std::string_view key, std::string_view s) {
  return setFilterState(key, s);
}

// Continue/Respond/Route
inline WasmResult continueDownstream() { return proxy_continue_stream(WasmStreamType::Downstream); }
inline WasmResult continueUpstream() { return proxy_continue_stream(WasmStreamType::Upstream); }

inline WasmResult closeDownstream() { return proxy_close_stream(WasmStreamType::Downstream); }
inline WasmResult closeUpstream() { return proxy_close_stream(WasmStreamType::Upstream); }

inline WasmResult continueRequest() { return proxy_continue_stream(WasmStreamType::Request); }
inline WasmResult continueResponse() { return proxy_continue_stream(WasmStreamType::Response); }

inline WasmResult closeRequest() { return proxy_close_stream(WasmStreamType::Request); }
inline WasmResult closeResponse() { return proxy_close_stream(WasmStreamType::Response); }

inline WasmResult sendLocalResponse(uint32_t response_code, std::string_view response_code_details,
                                    std::string_view body,
                                    const HeaderStringPairs &additional_response_headers,
                                    GrpcStatus grpc_status = GrpcStatus::InvalidCode) {
  const char *ptr = nullptr;
  size_t size = 0;
  exportPairs(additional_response_headers, &ptr, &size);
  WasmResult result = proxy_send_local_response(
      response_code, response_code_details.data(), response_code_details.size(), body.data(),
      body.size(), ptr, size, static_cast<uint32_t>(grpc_status));
  ::free(const_cast<char *>(ptr));
  return result;
}

// SharedData
inline WasmResult getSharedData(std::string_view key, WasmDataPtr *value, uint32_t *cas = nullptr) {
  uint32_t dummy_cas;
  const char *value_ptr = nullptr;
  size_t value_size = 0;
  if (!cas)
    cas = &dummy_cas;
  auto result = proxy_get_shared_data(key.data(), key.size(), &value_ptr, &value_size, cas);
  if (result != WasmResult::Ok) {
    return result;
  }
  *value = std::make_unique<WasmData>(value_ptr, value_size);
  return WasmResult::Ok;
}

inline WasmResult setSharedData(std::string_view key, std::string_view value, uint32_t cas = 0) {
  return proxy_set_shared_data(key.data(), key.size(), value.data(), value.size(), cas);
}

inline WasmDataPtr getSharedDataValue(std::string_view key, uint32_t *cas = nullptr) {
  WasmDataPtr data;
  auto result = getSharedData(key, &data, cas);
  if (result != WasmResult::Ok) {
    logAbort("getSharedData returned WasmError: " + toString(result));
  }
  return data;
}

// SharedQueue
inline WasmResult registerSharedQueue(std::string_view queue_name, uint32_t *token) {
  return proxy_register_shared_queue(queue_name.data(), queue_name.size(), token);
}

inline WasmResult resolveSharedQueue(std::string_view vm_id, std::string_view queue_name,
                                     uint32_t *token) {
  return proxy_resolve_shared_queue(vm_id.data(), vm_id.size(), queue_name.data(),
                                    queue_name.size(), token);
}

inline WasmResult enqueueSharedQueue(uint32_t token, std::string_view data) {
  return proxy_enqueue_shared_queue(token, data.data(), data.size());
}

inline WasmResult dequeueSharedQueue(uint32_t token, WasmDataPtr *data) {
  const char *data_ptr = nullptr;
  size_t data_size = 0;
  auto result = proxy_dequeue_shared_queue(token, &data_ptr, &data_size);
  *data = std::make_unique<WasmData>(data_ptr, data_size);
  return result;
}

// Headers/Trailers
inline WasmResult addHeaderMapValue(WasmHeaderMapType type, std::string_view key,
                                    std::string_view value) {
  return proxy_add_header_map_value(type, key.data(), key.size(), value.data(), value.size());
}

inline WasmDataPtr getHeaderMapValue(WasmHeaderMapType type, std::string_view key) {
  const char *value_ptr = nullptr;
  size_t value_size = 0;
  proxy_get_header_map_value(type, key.data(), key.size(), &value_ptr, &value_size);
  return std::make_unique<WasmData>(value_ptr, value_size);
}

inline WasmResult replaceHeaderMapValue(WasmHeaderMapType type, std::string_view key,
                                        std::string_view value) {
  return proxy_replace_header_map_value(type, key.data(), key.size(), value.data(), value.size());
}

inline WasmResult removeHeaderMapValue(WasmHeaderMapType type, std::string_view key) {
  return proxy_remove_header_map_value(type, key.data(), key.size());
}

inline WasmDataPtr getHeaderMapPairs(WasmHeaderMapType type) {
  const char *ptr = nullptr;
  size_t size = 0;
  proxy_get_header_map_pairs(type, &ptr, &size);
  return std::make_unique<WasmData>(ptr, size);
}

inline WasmResult setHeaderMapPairs(WasmHeaderMapType type, const HeaderStringPairs &pairs) {
  const char *ptr = nullptr;
  size_t size = 0;
  exportPairs(pairs, &ptr, &size);
  auto result = proxy_set_header_map_pairs(type, ptr, size);
  ::free(const_cast<char *>(ptr));
  return result;
}

inline WasmResult getHeaderMapSize(WasmHeaderMapType type, size_t *size) {
  return proxy_get_header_map_size(type, size);
}

inline WasmResult addRequestHeader(std::string_view key, std::string_view value) {
  return addHeaderMapValue(WasmHeaderMapType::RequestHeaders, key, value);
}
inline WasmDataPtr getRequestHeader(std::string_view key) {
  return getHeaderMapValue(WasmHeaderMapType::RequestHeaders, key);
}
inline WasmResult replaceRequestHeader(std::string_view key, std::string_view value) {
  return replaceHeaderMapValue(WasmHeaderMapType::RequestHeaders, key, value);
}
inline WasmResult removeRequestHeader(std::string_view key) {
  return removeHeaderMapValue(WasmHeaderMapType::RequestHeaders, key);
}
inline WasmDataPtr getRequestHeaderPairs() {
  return getHeaderMapPairs(WasmHeaderMapType::RequestHeaders);
}
inline WasmResult setRequestHeaderPairs(const HeaderStringPairs &pairs) {
  return setHeaderMapPairs(WasmHeaderMapType::RequestHeaders, pairs);
}
inline WasmResult getRequestHeaderSize(size_t *size) {
  return getHeaderMapSize(WasmHeaderMapType::RequestHeaders, size);
}

inline WasmResult addRequestTrailer(std::string_view key, std::string_view value) {
  return addHeaderMapValue(WasmHeaderMapType::RequestTrailers, key, value);
}
inline WasmDataPtr getRequestTrailer(std::string_view key) {
  return getHeaderMapValue(WasmHeaderMapType::RequestTrailers, key);
}
inline WasmResult replaceRequestTrailer(std::string_view key, std::string_view value) {
  return replaceHeaderMapValue(WasmHeaderMapType::RequestTrailers, key, value);
}
inline WasmResult removeRequestTrailer(std::string_view key) {
  return removeHeaderMapValue(WasmHeaderMapType::RequestTrailers, key);
}
inline WasmDataPtr getRequestTrailerPairs() {
  return getHeaderMapPairs(WasmHeaderMapType::RequestTrailers);
}
inline WasmResult setRequestTrailerPairs(const HeaderStringPairs &pairs) {
  return setHeaderMapPairs(WasmHeaderMapType::RequestTrailers, pairs);
}
inline WasmResult getRequestTrailerSize(size_t *size) {
  return getHeaderMapSize(WasmHeaderMapType::RequestTrailers, size);
}

inline WasmResult addResponseHeader(std::string_view key, std::string_view value) {
  return addHeaderMapValue(WasmHeaderMapType::ResponseHeaders, key, value);
}
inline WasmDataPtr getResponseHeader(std::string_view key) {
  return getHeaderMapValue(WasmHeaderMapType::ResponseHeaders, key);
}
inline WasmResult replaceResponseHeader(std::string_view key, std::string_view value) {
  return replaceHeaderMapValue(WasmHeaderMapType::ResponseHeaders, key, value);
}
inline WasmResult removeResponseHeader(std::string_view key) {
  return removeHeaderMapValue(WasmHeaderMapType::ResponseHeaders, key);
}
inline WasmDataPtr getResponseHeaderPairs() {
  return getHeaderMapPairs(WasmHeaderMapType::ResponseHeaders);
}
inline WasmResult setResponseHeaderPairs(const HeaderStringPairs &pairs) {
  return setHeaderMapPairs(WasmHeaderMapType::ResponseHeaders, pairs);
}
inline WasmResult getResponseHeaderSize(size_t *size) {
  return getHeaderMapSize(WasmHeaderMapType::ResponseHeaders, size);
}

inline WasmResult addResponseTrailer(std::string_view key, std::string_view value) {
  return addHeaderMapValue(WasmHeaderMapType::ResponseTrailers, key, value);
}
inline WasmDataPtr getResponseTrailer(std::string_view key) {
  return getHeaderMapValue(WasmHeaderMapType::ResponseTrailers, key);
}
inline WasmResult replaceResponseTrailer(std::string_view key, std::string_view value) {
  return replaceHeaderMapValue(WasmHeaderMapType::ResponseTrailers, key, value);
}
inline WasmResult removeResponseTrailer(std::string_view key) {
  return removeHeaderMapValue(WasmHeaderMapType::ResponseTrailers, key);
}
inline WasmDataPtr getResponseTrailerPairs() {
  return getHeaderMapPairs(WasmHeaderMapType::ResponseTrailers);
}
inline WasmResult setResponseTrailerPairs(const HeaderStringPairs &pairs) {
  return setHeaderMapPairs(WasmHeaderMapType::ResponseTrailers, pairs);
}
inline WasmResult getResponseTrailerSize(size_t *size) {
  return getHeaderMapSize(WasmHeaderMapType::ResponseTrailers, size);
}

// Buffer
inline WasmDataPtr getBufferBytes(WasmBufferType type, size_t start, size_t length) {
  const char *ptr = nullptr;
  size_t size = 0;
  proxy_get_buffer_bytes(type, start, length, &ptr, &size);
  return std::make_unique<WasmData>(ptr, size);
}

inline WasmResult getBufferStatus(WasmBufferType type, size_t *size, uint32_t *flags) {
  return proxy_get_buffer_status(type, size, flags);
}

inline WasmResult setBuffer(WasmBufferType type, size_t start, size_t length, std::string_view data,
                            size_t *new_size = nullptr) {
  auto result = proxy_set_buffer_bytes(type, start, length, data.data(), data.size());
  if (result == WasmResult::Ok && new_size)
    *new_size = *new_size - length + data.size();
  return result;
}

// HTTP

inline void MakeHeaderStringPairsBuffer(const HeaderStringPairs &headers, void **buffer_ptr,
                                        size_t *size_ptr) {
  if (headers.empty()) {
    *buffer_ptr = nullptr;
    *size_ptr = 0;
    return;
  }
  int size = 4; // number of headers
  for (auto &p : headers) {
    size += 8;                   // size of key, size of value
    size += p.first.size() + 1;  // null terminated key
    size += p.second.size() + 1; // null terminated value
  }
  char *buffer = static_cast<char *>(::malloc(size));
  char *b = buffer;
  *reinterpret_cast<int32_t *>(b) = headers.size();
  b += sizeof(int32_t);
  for (auto &p : headers) {
    *reinterpret_cast<int32_t *>(b) = p.first.size();
    b += sizeof(int32_t);
    *reinterpret_cast<int32_t *>(b) = p.second.size();
    b += sizeof(int32_t);
  }
  for (auto &p : headers) {
    memcpy(b, p.first.data(), p.first.size());
    b += p.first.size();
    *b++ = 0;
    memcpy(b, p.second.data(), p.second.size());
    b += p.second.size();
    *b++ = 0;
  }
  *buffer_ptr = buffer;
  *size_ptr = size;
}

inline WasmResult makeHttpCall(std::string_view uri, const HeaderStringPairs &request_headers,
                               std::string_view request_body,
                               const HeaderStringPairs &request_trailers,
                               uint32_t timeout_milliseconds, uint32_t *token_ptr) {
  void *headers_ptr = nullptr, *trailers_ptr = nullptr;
  size_t headers_size = 0, trailers_size = 0;
  MakeHeaderStringPairsBuffer(request_headers, &headers_ptr, &headers_size);
  MakeHeaderStringPairsBuffer(request_trailers, &trailers_ptr, &trailers_size);
  WasmResult result = proxy_http_call(uri.data(), uri.size(), headers_ptr, headers_size,
                                      request_body.data(), request_body.size(), trailers_ptr,
                                      trailers_size, timeout_milliseconds, token_ptr);
  ::free(headers_ptr);
  ::free(trailers_ptr);
  return result;
}

// Low level metrics interface.

inline WasmResult defineMetric(MetricType type, std::string_view name, uint32_t *metric_id) {
  return proxy_define_metric(type, name.data(), name.size(), metric_id);
}

inline WasmResult incrementMetric(uint32_t metric_id, int64_t offset) {
  return proxy_increment_metric(metric_id, offset);
}

inline WasmResult recordMetric(uint32_t metric_id, uint64_t value) {
  return proxy_record_metric(metric_id, value);
}

inline WasmResult getMetric(uint32_t metric_id, uint64_t *value) {
  return proxy_get_metric(metric_id, value);
}

// Higher level metrics interface.

struct MetricTag {
  enum class TagType : uint32_t {
    String = 0,
    Int = 1,
    Bool = 2,
  };
  std::string name;
  TagType tagType;
};

struct MetricBase {
  MetricBase(MetricType t, const std::string &n) : type(t), name(n) {}
  MetricBase(MetricType t, const std::string &n, const std::vector<MetricTag> &ts)
      : type(t), name(n), tags(ts.begin(), ts.end()) {}
  MetricBase(MetricType t, const std::string &n, const std::vector<MetricTag> &ts, std::string fs,
             std::string vs)
      : type(t), name(n), tags(ts.begin(), ts.end()), field_separator(fs), value_separator(vs) {}

  MetricType type;
  std::string name;
  std::string prefix;
  std::vector<MetricTag> tags;
  std::unordered_map<std::string, uint32_t> metric_ids;

  std::string field_separator = "."; // used to separate two fields.
  std::string value_separator = "."; // used to separate a field from its value.

  std::string prefixWithFields(const std::vector<std::string> &fields);
  uint32_t resolveFullName(const std::string &n);
  uint32_t resolveWithFields(const std::vector<std::string> &fields);
  void partiallyResolveWithFields(const std::vector<std::string> &fields);
  std::string nameFromIdSlow(uint32_t id);
};

struct Metric : public MetricBase {
  Metric(MetricType t, const std::string &n) : MetricBase(t, n) {}
  Metric(MetricType t, const std::string &n, const std::vector<MetricTag> &ts)
      : MetricBase(t, n, ts) {}
  Metric(MetricType t, const std::string &n, const std::vector<MetricTag> &ts,
         std::string field_separator, std::string value_separator)
      : MetricBase(t, n, ts, field_separator, value_separator) {}

  template <typename... Fields> void increment(int64_t offset, Fields... tags);
  template <typename... Fields> void record(uint64_t value, Fields... tags);
  template <typename... Fields> uint64_t get(Fields... tags);
  template <typename... Fields> uint32_t resolve(Fields... tags);
  template <typename... Fields> Metric partiallyResolve(Fields... tags);
};

inline std::string MetricBase::prefixWithFields(const std::vector<std::string> &fields) {
  size_t s = prefix.size();
  for (size_t i = 0; i < fields.size(); i++) {
    s += tags[i].name.size() + value_separator.size();
  }
  for (auto &f : fields) {
    s += f.size() + field_separator.size();
  }
  std::string n;
  n.reserve(s);
  n.append(prefix);
  for (size_t i = 0; i < fields.size(); i++) {
    n.append(tags[i].name);
    n.append(value_separator);
    n.append(fields[i]);
    n.append(field_separator);
  }
  return n;
}

inline uint32_t MetricBase::resolveWithFields(const std::vector<std::string> &fields) {
  if (fields.size() != tags.size()) {
#if WASM_EXCEPTIONS
    throw ProxyException("metric fields.size() != tags.size()");
#else
    logAbort("metric fields.size() != tags.size()");
#endif
  }
  return resolveFullName(prefixWithFields(fields) + name);
}

inline void MetricBase::partiallyResolveWithFields(const std::vector<std::string> &fields) {
  if (fields.size() >= tags.size()) {
#if WASM_EXCEPTIONS
    throw ProxyException("metric fields.size() >= tags.size()");
#else
    logAbort("metric fields.size() >= tags.size()");
#endif
  }
  prefix = prefixWithFields(fields);
  tags.erase(tags.begin(), tags.begin() + (fields.size()));
}

template <typename T> inline std::string toString(T t) { return std::to_string(t); }

template <> inline std::string toString(std::string_view t) { return std::string(t); }

template <> inline std::string toString(const char *t) { return std::string(t); }

template <> inline std::string toString(std::string t) { return t; }

template <> inline std::string toString(bool t) { return t ? "true" : "false"; }

template <typename T> struct StringToStringView { typedef T type; };

template <> struct StringToStringView<std::string> { typedef std::string_view type; };

inline uint32_t MetricBase::resolveFullName(const std::string &n) {
  auto it = metric_ids.find(n);
  if (it == metric_ids.end()) {
    uint32_t metric_id;
    CHECK_RESULT(defineMetric(type, n, &metric_id));
    metric_ids[n] = metric_id;
    return metric_id;
  }
  return it->second;
}

inline std::string MetricBase::nameFromIdSlow(uint32_t id) {
  for (auto &p : metric_ids)
    if (p.second == id)
      return p.first;
  return "";
}

template <typename... Fields> inline uint32_t Metric::resolve(Fields... f) {
  std::vector<std::string> fields{toString(f)...};
  return resolveWithFields(fields);
}

template <typename... Fields> Metric Metric::partiallyResolve(Fields... f) {
  std::vector<std::string> fields{toString(f)...};
  Metric partial_metric(*this);
  partial_metric.partiallyResolveWithFields(fields);
  return partial_metric;
}

template <typename... Fields> inline void Metric::increment(int64_t offset, Fields... f) {
  std::vector<std::string> fields{toString(f)...};
  auto metric_id = resolveWithFields(fields);
  incrementMetric(metric_id, offset);
}

template <typename... Fields> inline void Metric::record(uint64_t value, Fields... f) {
  std::vector<std::string> fields{toString(f)...};
  auto metric_id = resolveWithFields(fields);
  recordMetric(metric_id, value);
}

template <typename... Fields> inline uint64_t Metric::get(Fields... f) {
  std::vector<std::string> fields{toString(f)...};
  auto metric_id = resolveWithFields(fields);
  uint64_t value;
  CHECK_RESULT(getMetric(metric_id, &value));
  return value;
}

template <typename T> struct MetricTagDescriptor {
  MetricTagDescriptor(std::string_view n) : name(n) {}
  MetricTagDescriptor(const char *n) : name(n) {}
  typedef T type;
  std::string_view name;
};

template <typename T> inline MetricTag toMetricTag(const MetricTagDescriptor<T> &) { return {}; }

template <> inline MetricTag toMetricTag(const MetricTagDescriptor<const char *> &d) {
  return {std::string(d.name), MetricTag::TagType::String};
}

template <> inline MetricTag toMetricTag(const MetricTagDescriptor<std::string> &d) {
  return {std::string(d.name), MetricTag::TagType::String};
}

template <> inline MetricTag toMetricTag(const MetricTagDescriptor<std::string_view> &d) {
  return {std::string(d.name), MetricTag::TagType::String};
}

template <> inline MetricTag toMetricTag(const MetricTagDescriptor<int> &d) {
  return {std::string(d.name), MetricTag::TagType::Int};
}

template <> inline MetricTag toMetricTag(const MetricTagDescriptor<bool> &d) {
  return {std::string(d.name), MetricTag::TagType::Bool};
}

struct SimpleCounter {
  SimpleCounter(uint32_t id) : metric_id(id) {}

  void increment(int64_t offset) { recordMetric(metric_id, offset); }
  void record(int64_t offset) { increment(offset); }
  uint64_t get() {
    uint64_t value;
    CHECK_RESULT(getMetric(metric_id, &value));
    return value;
  }
  void operator++() { increment(1); }
  void operator++(int) { increment(1); }

  uint32_t metric_id;
};

struct SimpleGauge {
  SimpleGauge(uint32_t id) : metric_id(id) {}

  void record(uint64_t offset) { recordMetric(metric_id, offset); }
  uint64_t get() {
    uint64_t value;
    CHECK_RESULT(getMetric(metric_id, &value));
    return value;
  }

  uint32_t metric_id;
};

struct SimpleHistogram {
  SimpleHistogram(uint32_t id) : metric_id(id) {}

  void record(int64_t offset) { recordMetric(metric_id, offset); }

  uint32_t metric_id;
};

template <typename... Tags> struct Counter : public MetricBase {
  static Counter<Tags...> *New(std::string_view name, MetricTagDescriptor<Tags>... fieldnames);

  template <typename... T>
  Counter(std::string_view name, MetricTagDescriptor<T>... descriptors)
      : Counter<T...>(std::string(name), std::vector<MetricTag>({toMetricTag(descriptors)...})) {
  }

  SimpleCounter resolve(Tags... f) {
    std::vector<std::string> fields{toString(f)...};
    return SimpleCounter(resolveWithFields(fields));
  }

  template <typename... AdditionalTags>
  Counter<AdditionalTags...> *extendAndResolve(Tags... f,
                                               MetricTagDescriptor<AdditionalTags>... fieldnames) {
    std::vector<std::string> fields{toString(f)...};
    auto new_counter = Counter<AdditionalTags...>::New(name, fieldnames...);
    new_counter->prefix = prefixWithFields(fields);
    return new_counter;
  }

  void increment(int64_t offset, Tags... tags) {
    std::vector<std::string> fields{toString(tags)...};
    auto metric_id = resolveWithFields(fields);
    incrementMetric(metric_id, offset);
  }

  void record(int64_t offset, Tags... tags) { increment(offset, tags...); }

  uint64_t get(Tags... tags) {
    std::vector<std::string> fields{toString(tags)...};
    auto metric_id = resolveWithFields(fields);
    uint64_t value;
    CHECK_RESULT(getMetric(metric_id, &value));
    return value;
  }

private:
  Counter(const std::string &name, const std::vector<MetricTag> &tags)
      : MetricBase(MetricType::Counter, name, tags) {}
};

template <typename... Tags>
inline Counter<Tags...> *Counter<Tags...>::New(std::string_view name,
                                               MetricTagDescriptor<Tags>... descriptors) {
  return new Counter<Tags...>(std::string(name),
                              std::vector<MetricTag>({toMetricTag(descriptors)...}));
}

template <typename... Tags> struct Gauge : public MetricBase {
  static Gauge<Tags...> *New(std::string_view name, MetricTagDescriptor<Tags>... fieldnames);

  template <typename... T>
  Gauge(std::string_view name, MetricTagDescriptor<T>... descriptors)
      : Gauge<T...>(std::string(name), std::vector<MetricTag>({toMetricTag(descriptors)...})) {}

  SimpleGauge resolve(Tags... f) {
    std::vector<std::string> fields{toString(f)...};
    return SimpleGauge(resolveWithFields(fields));
  }

  template <typename... AdditionalTags>
  Gauge<AdditionalTags...> *extendAndResolve(Tags... f,
                                             MetricTagDescriptor<AdditionalTags>... fieldnames) {
    std::vector<std::string> fields{toString(f)...};
    auto new_gauge = Gauge<AdditionalTags...>::New(name, fieldnames...);
    new_gauge->prefix = prefixWithFields(fields);
    return new_gauge;
  }

  void record(int64_t offset, typename StringToStringView<Tags>::type... tags) {
    std::vector<std::string> fields{toString(tags)...};
    auto metric_id = resolveWithFields(fields);
    recordMetric(metric_id, offset);
  }

  uint64_t get(Tags... tags) {
    std::vector<std::string> fields{toString(tags)...};
    auto metric_id = resolveWithFields(fields);
    uint64_t value;
    CHECK_RESULT(getMetric(metric_id, &value));
    return value;
  }

private:
  Gauge(const std::string &name, const std::vector<MetricTag> &tags)
      : MetricBase(MetricType::Gauge, name, tags) {}
};

template <typename... Tags>
inline Gauge<Tags...> *Gauge<Tags...>::New(std::string_view name,
                                           MetricTagDescriptor<Tags>... descriptors) {
  return new Gauge<Tags...>(std::string(name),
                            std::vector<MetricTag>({toMetricTag(descriptors)...}));
}

template <typename... Tags> struct Histogram : public MetricBase {
  static Histogram<Tags...> *New(std::string_view name, MetricTagDescriptor<Tags>... fieldnames);

  template <typename... T>
  Histogram(std::string_view name, MetricTagDescriptor<T>... descriptors)
      : Histogram<T...>(std::string(name),
                           std::vector<MetricTag>({toMetricTag(descriptors)...})) {}

  SimpleHistogram resolve(Tags... f) {
    std::vector<std::string> fields{toString(f)...};
    return SimpleHistogram(resolveWithFields(fields));
  }

  template <typename... AdditionalTags>
  Histogram<AdditionalTags...> *
  extendAndResolve(Tags... f, MetricTagDescriptor<AdditionalTags>... fieldnames) {
    std::vector<std::string> fields{toString(f)...};
    auto new_histogram = Histogram<AdditionalTags...>::New(name, fieldnames...);
    new_histogram->prefix = prefixWithFields(fields);
    return new_histogram;
  }

  void record(int64_t offset, typename StringToStringView<Tags>::type... tags) {
    std::vector<std::string> fields{toString(tags)...};
    auto metric_id = resolveWithFields(fields);
    recordMetric(metric_id, offset);
  }

private:
  Histogram(const std::string &name, const std::vector<MetricTag> &tags)
      : MetricBase(MetricType::Histogram, name, tags) {}
};

template <typename... Tags>
inline Histogram<Tags...> *Histogram<Tags...>::New(std::string_view name,
                                                   MetricTagDescriptor<Tags>... descriptors) {
  return new Histogram<Tags...>(std::string(name),
                                std::vector<MetricTag>({toMetricTag(descriptors)...}));
}

inline WasmResult grpcCall(std::string_view service, std::string_view service_name,
                           std::string_view method_name, const HeaderStringPairs &initial_metadata,
                           std::string_view request, uint32_t timeout_milliseconds,
                           uint32_t *token_ptr) {
  void *metadata_ptr = nullptr;
  size_t metadata_size = 0;
  MakeHeaderStringPairsBuffer(initial_metadata, &metadata_ptr, &metadata_size);
  WasmResult result =
      proxy_grpc_call(service.data(), service.size(), service_name.data(), service_name.size(),
                      method_name.data(), method_name.size(), metadata_ptr, metadata_size,
                      request.data(), request.size(), timeout_milliseconds, token_ptr);
  ::free(metadata_ptr);
  return result;
}

#ifdef PROXY_WASM_PROTOBUF
inline WasmResult grpcCall(std::string_view service, std::string_view service_name,
                           std::string_view method_name, const HeaderStringPairs &initial_metadata,
                           const google::protobuf::MessageLite &request,
                           uint32_t timeout_milliseconds, uint32_t *token_ptr) {
  std::string serialized_request;
  if (!request.SerializeToString(&serialized_request)) {
    return WasmResult::SerializationFailure;
  }
  return grpcCall(service, service_name, method_name, initial_metadata, serialized_request,
                  timeout_milliseconds, token_ptr);
}
#endif

inline WasmResult grpcStream(std::string_view service, std::string_view service_name,
                             std::string_view method_name,
                             const HeaderStringPairs &initial_metadata, uint32_t *token_ptr) {
  void *metadata_ptr = nullptr;
  size_t metadata_size = 0;
  MakeHeaderStringPairsBuffer(initial_metadata, &metadata_ptr, &metadata_size);
  WasmResult result = proxy_grpc_stream(service.data(), service.size(), service_name.data(),
                                        service_name.size(), method_name.data(), method_name.size(),
                                        metadata_ptr, metadata_size, token_ptr);
  ::free(metadata_ptr);
  return result;
}

inline WasmResult grpcCancel(uint32_t token) { return proxy_grpc_cancel(token); }

inline WasmResult grpcClose(uint32_t token) { return proxy_grpc_close(token); }

inline WasmResult grpcSend(uint32_t token, std::string_view message, bool end_stream) {
  return proxy_grpc_send(token, message.data(), message.size(), end_stream ? 1 : 0);
}

inline WasmResult RootContext::httpCall(std::string_view uri,
                                        const HeaderStringPairs &request_headers,
                                        std::string_view request_body,
                                        const HeaderStringPairs &request_trailers,
                                        uint32_t timeout_milliseconds, HttpCallCallback callback) {
  uint32_t token = 0;
  auto result = makeHttpCall(uri, request_headers, request_body, request_trailers,
                             timeout_milliseconds, &token);
  if (result == WasmResult::Ok) {
    http_calls_[token] = std::move(callback);
  }
  return result;
}

inline void RootContext::onHttpCallResponse(uint32_t token, uint32_t headers, size_t body_size,
                                            uint32_t trailers) {
  auto it = http_calls_.find(token);
  if (it != http_calls_.end()) {
    it->second(headers, body_size, trailers);
    http_calls_.erase(token);
  }
}

inline WasmResult
RootContext::grpcSimpleCall(std::string_view service, std::string_view service_name,
                            std::string_view method_name, const HeaderStringPairs &initial_metadata,
                            std::string_view request, uint32_t timeout_milliseconds,
                            Context::GrpcSimpleCallCallback callback) {
  uint32_t token = 0;
  WasmResult result = grpcCall(service, service_name, method_name, initial_metadata, request,
                               timeout_milliseconds, &token);
  if (result == WasmResult::Ok) {
    asRoot()->simple_grpc_calls_[token] = std::move(callback);
  }
  return result;
}

inline void GrpcCallHandlerBase::cancel() {
  grpcCancel(token_);
  context_->grpc_calls_.erase(token_);
}

inline void GrpcStreamHandlerBase::reset() {
  grpcCancel(token_);
  context_->grpc_streams_.erase(token_);
}

inline void GrpcStreamHandlerBase::close() {
  grpcClose(token_);
  local_close_ = true;
  if (local_close_ && remote_close_) {
    context_->grpc_streams_.erase(token_);
  }
  // NB: else callbacks can still occur: reset() to prevent further callbacks.
}

inline WasmResult GrpcStreamHandlerBase::send(std::string_view message, bool end_of_stream) {
  WasmResult r = grpcSend(token_, message, end_of_stream);
  if (r != WasmResult::Ok) {
    return r;
  }
  if (end_of_stream) {
    // NB: callbacks can still occur: reset() to prevent further callbacks.
    local_close_ = local_close_ || end_of_stream;
    if (local_close_ && remote_close_) {
      context_->grpc_streams_.erase(token_);
    }
  }
  return WasmResult::Ok;
}

inline void RootContext::onGrpcReceiveInitialMetadata(uint32_t token, uint32_t headers) {
  {
    auto it = grpc_streams_.find(token);
    if (it != grpc_streams_.end()) {
      it->second->onReceiveInitialMetadata(headers);
      return;
    }
  }
}

inline void RootContext::onGrpcReceiveTrailingMetadata(uint32_t token, uint32_t trailers) {
  {
    auto it = grpc_streams_.find(token);
    if (it != grpc_streams_.end()) {
      it->second->onReceiveTrailingMetadata(trailers);
      return;
    }
  }
}

inline void RootContext::onGrpcReceive(uint32_t token, size_t body_size) {
  {
    auto it = simple_grpc_calls_.find(token);
    if (it != simple_grpc_calls_.end()) {
      it->second(GrpcStatus::Ok, body_size);
      simple_grpc_calls_.erase(token);
      return;
    }
  }
  {
    auto it = grpc_calls_.find(token);
    if (it != grpc_calls_.end()) {
      it->second->onSuccess(body_size);
      grpc_calls_.erase(token);
      return;
    }
  }
  {
    auto it = grpc_streams_.find(token);
    if (it != grpc_streams_.end()) {
      it->second->onReceive(body_size);
      return;
    }
  }
}

inline void GrpcStreamHandlerBase::doRemoteClose(GrpcStatus status) {
  auto context = context_;
  auto token = token_;
  this->onRemoteClose(status);
  if (context->grpc_streams_.find(token) != context->grpc_streams_.end()) {
    // We have not been deleted, e.g. by reset() in the onRemoteCall() virtual
    // handler.
    remote_close_ = true;
    if (local_close_ && remote_close_) {
      context_->grpc_streams_.erase(token_);
    }
    // else do not erase the token since we can still send in this state.
  }
}

inline void RootContext::onGrpcClose(uint32_t token, GrpcStatus status) {
  {
    auto it = simple_grpc_calls_.find(token);
    if (it != simple_grpc_calls_.end()) {
      it->second(status, 0);
      simple_grpc_calls_.erase(token);
      return;
    }
  }
  {
    auto it = grpc_calls_.find(token);
    if (it != grpc_calls_.end()) {
      it->second->onFailure(status);
      grpc_calls_.erase(token);
      return;
    }
  }
  {
    auto it = grpc_streams_.find(token);
    if (it != grpc_streams_.end()) {
      it->second->doRemoteClose(status);
      return;
    }
  }
}

inline WasmResult RootContext::grpcCallHandler(
    std::string_view service, std::string_view service_name, std::string_view method_name,
    const HeaderStringPairs &initial_metadata, std::string_view request,
    uint32_t timeout_milliseconds, std::unique_ptr<GrpcCallHandlerBase> handler) {
  uint32_t token = 0;
  auto result = grpcCall(service, service_name, method_name, initial_metadata, request,
                         timeout_milliseconds, &token);
  if (result == WasmResult::Ok) {
    handler->token_ = token;
    handler->context_ = this;
    grpc_calls_[token] = std::move(handler);
  }
  return result;
}

inline WasmResult RootContext::grpcStreamHandler(std::string_view service,
                                                 std::string_view service_name,
                                                 std::string_view method_name,
                                                 const HeaderStringPairs &initial_metadata,
                                                 std::unique_ptr<GrpcStreamHandlerBase> handler) {
  uint32_t token = 0;
  auto result = grpcStream(service, service_name, method_name, initial_metadata, &token);
  if (result == WasmResult::Ok) {
    handler->token_ = token;
    handler->context_ = this;
    grpc_streams_[token] = std::move(handler);
  }
  return result;
}

inline WasmResult ContextBase::setEffectiveContext() { return proxy_set_effective_context(id_); }

inline uint64_t getCurrentTimeNanoseconds() {
  uint64_t t;
  CHECK_RESULT(proxy_get_current_time_nanoseconds(&t));
  return t;
}

inline void RootContext::done() { proxy_done(); }
