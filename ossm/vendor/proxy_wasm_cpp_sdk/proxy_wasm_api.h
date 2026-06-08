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

// Macro to log a message and abort the plugin if the given value is not
// `WasmResult::Ok`.
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

// Functions to log messages at various log levels.
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

// Logs a message at `LogLevel::critical` and aborts plugin execution.
inline void logAbort(std::string_view logMessage) {
  logCritical(logMessage);
  abort();
}

// Macro to log a message at the given log level with source file, line number,
// and function name included in the log message.
#define LOG(_level, ...)                                                                           \
  log##_level(std::string("[") + __FILE__ + ":" + std::to_string(__LINE__) +                       \
              "]::" + __FUNCTION__ + "() " + __VA_ARGS__)

// Macros to log messages at various log levels with source file, line number,
// and function name included in the log message.
#define LOG_TRACE(...) LOG(Trace, __VA_ARGS__)
#define LOG_DEBUG(...) LOG(Debug, __VA_ARGS__)
#define LOG_INFO(...) LOG(Info, __VA_ARGS__)
#define LOG_WARN(...) LOG(Warn, __VA_ARGS__)
#define LOG_ERROR(...) LOG(Error, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG(Critical, __VA_ARGS__)

// Buffers coming into the WASM filter.
class WasmData {
public:
  // Constructs a buffer that owns `size` bytes starting at `data`.
  WasmData(const char *data, size_t size) : data_(data), size_(size) {}
  // Frees buffer data.
  ~WasmData() { ::free(const_cast<char *>(data_)); }
  // Returns pointer to the start of the buffer;
  const char *data() { return data_; }
  // Returns the size of the buffer in bytes.
  size_t size() { return size_; }
  // Returns buffer data in the form of a string_view.
  std::string_view view() { return {data_, size_}; }
  // Returns a string copy of buffer data.
  std::string toString() { return std::string(view()); }
  // Returns a series of string pairs decoded from and backed by the buffer.
  std::vector<std::pair<std::string_view, std::string_view>> pairs();
  // Returns a protobuf of type T parsed from buffer contents.
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

// Returns the number of bytes used by the marshalled representation of
// `result`.
template <typename Pairs> size_t pairsSize(const Pairs &result) {
  size_t size = 4; // number of headers
  for (auto &p : result) {
    size += 8;                   // size of key, size of value
    size += p.first.size() + 1;  // null terminated key
    size += p.second.size() + 1; // null terminated value
  }
  return size;
}

// Marshals `pairs` to the memory buffer `buffer`. `Pairs` is a map-like type
// that provides a `size` method and iteration over elements that are
// `std::pair`s of `std::string` or `std::string_view`s.
template <typename Pairs> void marshalPairs(const Pairs &pairs, char *buffer) {
  char *b = buffer;
  *reinterpret_cast<uint32_t *>(b) = pairs.size();
  b += sizeof(uint32_t);
  for (auto &p : pairs) {
    *reinterpret_cast<uint32_t *>(b) = p.first.size();
    b += sizeof(uint32_t);
    *reinterpret_cast<uint32_t *>(b) = p.second.size();
    b += sizeof(uint32_t);
  }
  for (auto &p : pairs) {
    memcpy(b, p.first.data(), p.first.size());
    b += p.first.size();
    *b++ = 0;
    memcpy(b, p.second.data(), p.second.size());
    b += p.second.size();
    *b++ = 0;
  }
}

// Marshals `pairs` to a newly allocated memory buffer, returning the address of
// the buffer and its size in bytes via the `ptr` and `size_ptr` out-params.
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

// Hasher for pairs.
struct PairHash {
  template <typename T, typename U> std::size_t operator()(const std::pair<T, U> &x) const {
    return std::hash<T>()(x.first) + std::hash<U>()(x.second);
  }
};

// Hasher for three-element tuples.
struct Tuple3Hash {
  template <typename T, typename U, typename V>
  std::size_t operator()(const std::tuple<T, U, V> &x) const {
    return std::hash<T>()(std::get<0>(x)) + std::hash<U>()(std::get<1>(x)) +
           std::hash<V>()(std::get<2>(x));
  }
};

// Type for representing HTTP headers or metadata as string pairs.
using HeaderStringPairs = std::vector<std::pair<std::string, std::string>>;

// Superclass for handlers that receive events for unary gRPC callouts initiated
// by `RootContext::grpcCallHandler`.
class GrpcCallHandlerBase {
public:
  GrpcCallHandlerBase() {}
  virtual ~GrpcCallHandlerBase() {}

  // Returns the `RootContext` associated with the gRPC call.
  RootContext *context() { return context_; }

  // Cancels the gRPC call.
  void cancel();
  // Returns the token used to identify the gRPC call.
  uint32_t token() { return token_; }

  // Callback invoked on gRPC call success. `body_size` indicates the size in
  // bytes of the gRPC response body.
  virtual void onSuccess(size_t body_size) = 0;
  // Callback invoked on gRPC call failure. `status` conveys gRPC call status.
  virtual void onFailure(GrpcStatus status) = 0;

private:
  friend class RootContext;

  RootContext *context_{nullptr};
  uint32_t token_;
};

// Superclass for handlers that receive events for unary gRPC callouts initiated
// by `RootContext::grpcCallHandler`, templated on protobuf message type.
template <typename Message> class GrpcCallHandler : public GrpcCallHandlerBase {
public:
  GrpcCallHandler() : GrpcCallHandlerBase() {}
  virtual ~GrpcCallHandler() {}

  // Callback invoked on gRPC call success. `body_size` indicates the size in
  // bytes of the gRPC response body.
  virtual void onSuccess(size_t body_size) = 0;
};

// Superclass for handlers that receive events for streaming gRPC callouts
// initiated by `RootContext::grpcStreamHandler`.
class GrpcStreamHandlerBase {
public:
  GrpcStreamHandlerBase() {}
  virtual ~GrpcStreamHandlerBase() {}

  // Returns the `RootContext` associated with the gRPC call.
  RootContext *context() { return context_; }

  // Sends `message` over the gRPC stream. `end_of_stream` indicates whether
  // this is the last message to be written on the stream.
  //
  // Note that even with `end_of_stream == true`, callbacks can still
  // occur. Call `reset` to prevent further callbacks.
  WasmResult send(std::string_view message, bool end_of_stream);
  // Closes the gRPC stream.
  //
  // Note that callbacks can still occur after this method is called. Call
  // `reset` to prevent further callbacks.
  void close();
  // Resets the handler and prevents further callbacks.
  void reset();
  // Returns the token used to identify the gRPC call.
  uint32_t token() { return token_; }

  // Callback invoked when gRPC initial metadata is received.
  virtual void onReceiveInitialMetadata(uint32_t /* headers */) {}
  // Callback invoked when gRPC trailing metadata is recevied.
  virtual void onReceiveTrailingMetadata(uint32_t /* trailers */) {}
  // Callback invoked when gRPC stream data is received. `body_size` gives the
  // number of bytes received. The actual bytes can be retrieved by calling
  // `getBufferBytes` with `WasmBufferType::GrpcReceiveBuffer`.
  virtual void onReceive(size_t body_size) = 0;
  // Callback invoked when the remote peer closed the gRPC stream. `status`
  // gives the gRPC call status.
  virtual void onRemoteClose(GrpcStatus status) = 0;

protected:
  friend class RootContext;

  void doRemoteClose(GrpcStatus status);

  bool local_close_{false};
  bool remote_close_{false};
  RootContext *context_{nullptr};
  uint32_t token_;
};

// Superclass for handlers that receive events for streaming gRPC callouts
// initiated by `RootContext::grpcStreamHandler`, templated on protobuf message
// types.
template <typename Request, typename Response>
class GrpcStreamHandler : public GrpcStreamHandlerBase {
public:
  GrpcStreamHandler() : GrpcStreamHandlerBase() {}
  virtual ~GrpcStreamHandler() {}

  // Sends `message` over the gRPC stream. `end_of_stream` indicates whether
  // this is the last message to be written on the stream.
  WasmResult send(const Request &message, bool end_of_stream) {
    std::string output;
    if (!message.SerializeToString(&output)) {
      return WasmResult::SerializationFailure;
    }
    GrpcStreamHandlerBase::send(output, end_of_stream);
    local_close_ = local_close_ || end_of_stream;
    return WasmResult::Ok;
  }

  // Callback invoked when gRPC stream data is received. `body_size` gives the
  // number of bytes received. The actual bytes can be retrieved by calling
  // `getBufferBytes` with `WasmBufferType::GrpcReceiveBuffer`.
  virtual void onReceive(size_t body_size) = 0;
};

// Behavior supported by all contexts.
class ContextBase {
public:
  // Constructs a context identified by `id` in underlying ABI calls.
  explicit ContextBase(uint32_t id) : id_(id) {}
  virtual ~ContextBase() {}

  // Returns numeric ID that identifies this context in ABI calls.
  uint32_t id() { return id_; }

  // Makes this context the effective context for calls out of the VM.
  WasmResult setEffectiveContext();

  // If this context is a root context, return it as a `RootContext`, else
  // return nullptr.
  virtual RootContext *asRoot() { return nullptr; }

  // If this context is a stream context, return it as a `Context`, else return
  // nullptr.
  virtual Context *asContext() { return nullptr; }

  // Callback invoked when the context is created.
  virtual void onCreate() {}

  // Callback invoked when the host is done with a context. Returning true
  // indicates that the host can proceed to delete the context and perform other
  // cleanup. Returning false indicates that the context is still being used,
  // and the plugin will call `RootContext::done` later to inform the host that
  // the context can be deleted.
  virtual bool onDoneBase() = 0;

  // Callback invoked after the host is done with a context, but before deleting
  // it, when either `onDoneBase` has returned true, or `the plugin has called
  // RootContext::done`. This callback can be used for generating log entries.
  virtual void onLog() {}

  // Callback invoked when the host is releasing its state associated with the
  // context. No further callbacks will be invoked on the context after this
  // callback.
  virtual void onDelete() {} // Called when the stream or VM is being deleted.

  // Callback invoked when a foreign function event
  // arrives. `foreign_function_id` indicates the event type, which is some
  // value agreed upon out-of-band by host and plugin. `data_size` gives the
  // size of argument data for the call, in bytes. This argument data can be
  // retrieved by calling `getBufferBytes` with `WasmBufferType::CallData`.
  virtual void onForeignFunction(uint32_t /* foreign_function_id */, uint32_t /* data_size */) {}

  // Returns the host's currently configured log level via the `level`
  // out-param.
  WasmResult getLogLevel(LogLevel *level) { return proxy_get_log_level(level); }

  // Type alias for callbacks that are invoked when an outbound HTTP call
  // initiated via `RootContext::httpCall` completes. `num_headers` gives the
  // number of response header fields, `body_size` gives the size of the
  // response body in bytes, and `num_trailers` gives the number of response
  // trailer fields.
  using HttpCallCallback =
      std::function<void(uint32_t num_headers, size_t body_size, uint32_t num_trailers)>;

  // Type alias for callbacks that are invoked when an outbound gRPC call
  // initiated via `RootContext::grpcSimpleCall` completes. `status` gives the
  // gRPC call status, and `body_size` gives the size of the gRPC response body
  // in bytes.
  using GrpcSimpleCallCallback = std::function<void(GrpcStatus status, size_t body_size)>;

private:
  uint32_t id_;
};

// Behavior and operations supported by root contexts.
class RootContext : public ContextBase {
public:
  // Constructs a root context identified by `id` in underlying ABI
  // calls. `root_id` is a name that can be used to distinguish between
  // different root contexts used by a plugin, if there are multiple.
  RootContext(uint32_t id, std::string_view root_id) : ContextBase(id), root_id_(root_id) {}
  ~RootContext() {}

  // Returns ID that distinguishes this root context from others in use.
  std::string_view root_id() { return root_id_; }

  RootContext *asRoot() override { return this; }
  Context *asContext() override { return nullptr; }

  // Callback that may be invoked (e.g. by the control plane) to validate plugin
  // configuration. `configuration_size` gives the size of configuration data in
  // bytes, which can be retrieved by calling `getBufferBytes` with
  // `WasmBufferType::PluginConfiguration`. Returns true if the configuration is
  // valid, or false if invalid.
  virtual bool validateConfiguration(size_t /* configuration_size */) { return true; }

  // Callback invoked when the plugin is initially loaded, as well as whenever
  // configuration changes. `configuration_size` gives the size of configuration
  // data in bytes, which can be retrieved by calling `getBufferBytes` with
  // `WasmBufferType::PluginConfiguration`. Returns true if the configuration is
  // valid, or false if invalid.
  virtual bool onConfigure(size_t /* configuration_size */) { return true; }

  // Callback invoked when the plugin is started. `vm_configuration_size` gives
  // the size of VM configuration data in bytes, which can be retrieved by
  // calling `getBufferBytes` with `WasmBufferType::VmConfiguration`. Returns
  // true if the configuration is valid, or false if invalid.
  virtual bool onStart(size_t /* vm_configuration_size */) { return true; }

  // Callback invoked the timer configured by a previous call to
  // `proxy_set_tick_period_milliseconds` goes off.
  virtual void onTick() {}

  // Callback invoked when data arrives on a shared queue. `token` is a value
  // that identifies the shared queue, previously established in a call to
  // `registerSharedQueue` or `resolveSharedQueue`. Shared queue data can be
  // retrieved by calling `dequeueSharedQueue` with the given `token` value.
  virtual void onQueueReady(uint32_t /* token */) {}

  // Callback invoked when the plugin is being stopped. Returning true indicates
  // that the host can proceed to delete the context and perform other
  // cleanup. Returning false indicates that the context is still being used,
  // and the plugin will call `RootContext::done` later to inform the host that
  // the context can be deleted.
  virtual bool onDone() { return true; }

  // Informs the host that it is safe to delete this context, following an
  // earlier `onDone` callback on it that had returned false.
  void done();

  // Low-level callback invoked when the response for an outbound HTTP call
  // initiated via `makeHttpCall` is received, or the HTTP call times
  // out. Plugins that use the high-level `RootContext::httpCall` API to perform
  // outbound HTTP calls should not override this method, as doing so
  // effectively disables the response callback for that method.
  //
  // `token` associates the response with the corresponding `makeHttpCall`
  // call. `headers` is the number of HTTP header fields, or 0 if the request
  // timed out. `body_size` is the size of the response body in bytes, and
  // `trailers` is the number of HTTP trailer fields.
  //
  // Header and trailer values can be fetched by calling `getHeaderMapValue`
  // with `WasmHeaderMapType::HttpCallResponseHeaders` and
  // `WasmHeaderMapType::HttpCallResponseTrailers`, respectively. Response body
  // bytes can be fetched by calling `getBufferBytes` with
  // `WasmBufferType::HttpCallResponseBody`.
  virtual void onHttpCallResponse(uint32_t token, uint32_t headers, size_t body_size,
                                  uint32_t trailers);

  // Low-level callback invoked when initial metadata for an outbound gRPC call
  // initiated via `grpcCall` or `grpcStream` is received. Plugins that use the
  // the high-level `RootContext::grpcSimpleCall` or
  // `RootContext::grpcCallHandler` API to perform outbound gRPC calls should
  // not override this method, as doing so effectively disables the response
  // callbacks for those methods.
  //
  // `token` associates the received metadata with the corresponding `grpcCall`
  // or `grpcStream` call. `headers` is the number of metadata fields received,
  // which can be retrieved by calling `getHeaderMapValue` with
  // `WasmHeaderMapType::GrpcReceiveInitialMetadata`.
  virtual void onGrpcReceiveInitialMetadata(uint32_t token, uint32_t headers);

  // Low-level callback invoked when trailing metadata for an outbound gRPC call
  // initiated via `grpcCall` or `grpcStream` is received. Plugins that use the
  // the high-level `RootContext::grpcSimpleCall` or
  // `RootContext::grpcCallHandler` API to perform outbound gRPC calls should
  // not override this method, as doing so effectively disables the response
  // callbacks for those methods.
  //
  // `token` associates the received metadata with the corresponding `grpcCall`
  // or `grpcStream` call. `trailers` is the number of metadata fields received,
  // which can be retrieved by calling `getHeaderMapValue` with
  // `WasmHeaderMapType::GrpcReceiveTrailingMetadata`.
  virtual void onGrpcReceiveTrailingMetadata(uint32_t token, uint32_t trailers);

  // Low-level callback invoked when response data for an outbound gRPC call
  // initiated via `grpcCall` or `grpcStream` is received. Plugins that use the
  // the high-level `RootContext::grpcSimpleCall` or
  // `RootContext::grpcCallHandler` API to perform outbound gRPC calls should
  // not override this method, as doing so effectively disables the response
  // callbacks for those methods.
  //
  // `token` associates the received metadata with the corresponding `grpcCall`
  // or `grpcStream` call. `body_size` is the number of response body bytes
  // received, which can be retrieved by calling `getBuffer` with
  // `WasmHeaderMapType::GrpcReceiveBuffer`.
  virtual void onGrpcReceive(uint32_t token, size_t body_size);

  // Low-level callback invoked when an outbound gRPC call initiated via
  // `grpcCall` or `grpcStream` terminates. Plugins that use the the high-level
  // `RootContext::grpcSimpleCall` or `RootContext::grpcCallHandler` API to
  // perform outbound gRPC calls should not override this method, as doing so
  // effectively disables the response callbacks for those methods.
  //
  // `token` associates the received metadata with the corresponding `grpcCall`
  // or `grpcStream` call. `status` gives the gRPC status code for the call.
  virtual void onGrpcClose(uint32_t token, GrpcStatus status);

  // Initiates an outbound HTTP call to URI `uri`, sending `request_headers`,
  // `request_body`, and `request_trailers` as the request headers, body, and
  // trailers. `callback` is a callback that is invoked when the HTTP response
  // is received, or the request times out.  Returns `WasmResult::Ok` if the
  // request is successfully sent.
  WasmResult httpCall(std::string_view uri, const HeaderStringPairs &request_headers,
                      std::string_view request_body, const HeaderStringPairs &request_trailers,
                      uint32_t timeout_milliseconds, HttpCallCallback callback);

  // Initiates an outbound unary gRPC call to the service named by `service` and
  // `service_name`, invoking method `method_name`. `initial_metadata` and
  // `request` specify the initial metadata and request body for the
  // call. `callback` is a callback that is invoked when the gRPC response is
  // received, or the request times out. Returns `WasmResult::Ok` if the call is
  // successfully sent.
  WasmResult grpcSimpleCall(std::string_view service, std::string_view service_name,
                            std::string_view method_name, const HeaderStringPairs &initial_metadata,
                            std::string_view request, uint32_t timeout_milliseconds,
                            GrpcSimpleCallCallback callback);

  // Initiates an outbound unary gRPC call to the service named by `service` and
  // `service_name`, invoking method `method_name`. `initial_metadata` and
  // `request` specify the initial metadata and request body for the
  // call. `success_callback` is a callback that is invoked when a successful
  // gRPC response is received. `failure_callback` is a callback that is invoked
  // if the gRPC call fails.  Returns `WasmResult::Ok` if the call is
  // successfully sent.
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

  // Initiates an outbound unary gRPC call to the service named by `service` and
  // `service_name`, invoking method `method_name`. `initial_metadata` and
  // `request` specify the initial metadata and request body for the
  // call. `handler` is a handler object that receives callbacks for gRPC call
  // events such as success or failure. Returns `WasmResult::Ok` if the call is
  // successfully sent.
  WasmResult grpcCallHandler(std::string_view service, std::string_view service_name,
                             std::string_view method_name,
                             const HeaderStringPairs &initial_metadata, std::string_view request,
                             uint32_t timeout_milliseconds,
                             std::unique_ptr<GrpcCallHandlerBase> handler);

#ifdef PROXY_WASM_PROTOBUF
  // Initiates an outbound unary gRPC call to the service named by `service` and
  // `service_name`, invoking method `method_name`. `initial_metadata` and
  // `request` specify the initial metadata and request protobuf for the
  // call. `callback` is a callback that is invoked when the gRPC response is
  // received, or the request times out. Returns `WasmResult::Ok` if the call is successfully sent.
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

  // Initiates an outbound unary gRPC call to the service named by `service` and
  // `service_name`, invoking method `method_name`. `initial_metadata` and
  // `request` specify the initial metadata and request protobuf for the
  // call. `success_callback` is a callback that is invoked when a successful
  // gRPC response is received. `failure_callback` is a callback that is invoked
  // if the gRPC call fails. Returns `WasmResult::Ok` if the call is
  // successfully sent.
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

  // Initiates an outbound unary gRPC call to the service named by `service` and
  // `service_name`, invoking method `method_name`. `initial_metadata` and
  // `request` specify the initial metadata and request protobuf for the
  // call. `handler` is a handler object that receives callbacks for gRPC call
  // events such as success or failure. Returns `WasmResult::Ok` if the call is
  // successfully sent.
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

  // Initiates an outbound streaming gRPC call to the service named by `service`
  // and `service_name`, invoking method `method_name`. `initial_metadata`
  // specifies the initial metadata for the call. `handler` is a handler object
  // that can be used to send messages over the gRPC stream, and receives
  // callbacks for gRPC call events such as receiving metadata and response
  // messages. Returns `WasmResult::Ok` if the call is successfully sent.
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

// Returns `RootContext` object for the root context named by `root_id`, or
// nullptr if none is found.
RootContext *getRoot(std::string_view root_id);

// Behavior and operations supported by stream contexts.
class Context : public ContextBase {
public:
  // Constructs a stream context identified by `id` in underlying ABI calls,
  // associated with root context `root`.
  Context(uint32_t id, RootContext *root) : ContextBase(id), root_(root) {}
  virtual ~Context() {}

  // Returns the root context associated with this stream context.
  RootContext *root() { return root_; }

  RootContext *asRoot() override { return nullptr; }
  Context *asContext() override { return this; }

  // Callback invoked when a new TCP(-like) connection is established. Returns
  // `FilterStatus` indicating whether processing of the connection should
  // continue or pause until a later call to `continueDownstream`.
  virtual FilterStatus onNewConnection() { return FilterStatus::Continue; }

  // Callback invoked when new data is received from downstream on a TCP(-like)
  // connection. `data_size` gives the number of bytes received, and
  // `end_of_stream` indicates whether this is the last data from
  // downstream. Data can be retrieved by calling `getBufferBytes` with
  // `WasmBufferType::NetworkDownstreamData`.
  //
  // Returns `FilterStatus` indicating whether processing of the connection
  // should continue or pause until a later call to `continueDownstream`.
  virtual FilterStatus onDownstreamData(size_t /* data_size */, bool /* end_of_stream */) {
    return FilterStatus::Continue;
  }

  // Callback invoked when new data is received from upstream on a TCP(-like)
  // connection. `data_size` gives the number of bytes received, and
  // `end_of_stream` indicates whether this is the last data from
  // downstream. Data can be retrieved by calling `getBufferBytes` with
  // `WasmBufferType::NetworkUpstreamData`.
  //
  // Returns `FilterStatus` indicating whether processing of the connection
  // should continue or pause until a later call to `continueUpstream`.
  virtual FilterStatus onUpstreamData(size_t /* data_size */, bool /* end_of_stream */) {
    return FilterStatus::Continue;
  }

  // Callback invoked when the downstream direction of a TCP(-like) connection
  // is closed. `close_type` indicates whether the connection was closed by the
  // proxy or the remote peer.
  virtual void onDownstreamConnectionClose(CloseType /* close_type */) {}

  // Callback invoked when the upstream direction of a TCP(-like) connection is
  // closed. `close_type` indicates whether the connection was closed by the
  // proxy or the remote peer.
  virtual void onUpstreamConnectionClose(CloseType /* close_type */) {}

  // Callback invoked when HTTP request headers are received. `headers` is the
  // number of header fields. `end_of_stream` indicates whether the request ends
  // immediately after the headers. Request header values can be accessed and
  // manipulated via `*RequestHeader*` hostcalls, or by specifying
  // `WasmHeaderMapType::RequestHeaders` in `*HeaderMap*` hostcalls.
  //
  // Returns `FilterHeadersStatus` indicating whether processing of the
  // connection should continue or pause until a later call to
  // `continueRequest`.
  virtual FilterHeadersStatus onRequestHeaders(uint32_t /* headers */, bool /* end_of_stream */) {
    return FilterHeadersStatus::Continue;
  }

  // Callback invoked when request metadata is received. `elements` is the
  // number of metadata entries. Metadata values are not currently accessible.
  virtual FilterMetadataStatus onRequestMetadata(uint32_t /* elements */) {
    return FilterMetadataStatus::Continue;
  }

  // Callback invoked when HTTP request body data is
  // received. `body_buffer_length` is the size of body data in
  // bytes. `end_of_stream` indicates whether the request ends immediately after
  // the request body data. Request body bytes can be retrieved by calling
  // `getBufferBytes` with `WasmBufferType::HttpRequestBody`.
  //
  // Returns `FilterDataStatus` indicating whether processing of the connection
  // should continue or pause until a later call to `continueRequest`.
  virtual FilterDataStatus onRequestBody(size_t /* body_buffer_length */,
                                         bool /* end_of_stream */) {
    return FilterDataStatus::Continue;
  }

  // Callback invoked when HTTP request trailers are received. `trailers` is the
  // number of trailer fields. Request trailer values can be accessed and
  // manipulated via `*RequestTrailer*` hostcalls, or by specifying
  // `WasmHeaderMapType::RequestTrailers` in `*HeaderMap*` hostcalls.
  //
  // Returns `FilterTrailersStatus` indicating whether processing of the
  // connection should continue or pause until a later call to
  // `continueRequest`.
  virtual FilterTrailersStatus onRequestTrailers(uint32_t /* trailers */) {
    return FilterTrailersStatus::Continue;
  }

  // Callback invoked when HTTP response headers are received. `headers` is the
  // number of header fields. `end_of_stream` indicates whether the response
  // ends immediately after the headers. Response header values can be accessed
  // and manipulated via `*ResponseHeader*` hostcalls, or by specifying
  // `WasmHeaderMapType::ResponseHeaders` in `*HeaderMap*` hostcalls.
  //
  // Returns `FilterHeadersStatus` indicating whether processing of the
  // connection should continue or pause until a later call to
  // `continueResponse`.
  virtual FilterHeadersStatus onResponseHeaders(uint32_t /* headers */, bool /* end_of_stream */) {
    return FilterHeadersStatus::Continue;
  }

  // Callback invoked when response metadata is received. `elements` is the
  // number of metadata entries. Metadata values are not currently accessible.
  virtual FilterMetadataStatus onResponseMetadata(uint32_t /* elements */) {
    return FilterMetadataStatus::Continue;
  }

  // Callback invoked when HTTP response body data is
  // received. `body_buffer_length` is the size of body data in
  // bytes. `end_of_stream` indicates whether the response ends immediately
  // after the response body data. Response body bytes can be retrieved by
  // calling `getBufferBytes` with `WasmBufferType::HttpResponseBody`.
  //
  // Returns `FilterDataStatus` indicating whether processing of the connection
  // should continue or pause until a later call to `continueResponse`.
  virtual FilterDataStatus onResponseBody(size_t /* body_buffer_length */,
                                          bool /* end_of_stream */) {
    return FilterDataStatus::Continue;
  }

  // Callback invoked when HTTP response trailers are received. `trailers` is
  // the number of trailer fields. Response trailer values can be accessed and
  // manipulated via `*ResponseTrailer*` hostcalls, or by specifying
  // `WasmHeaderMapType::ResponseTrailers` in `*HeaderMap*` hostcalls.
  //
  // Returns `FilterTrailersStatus` indicating whether processing of the
  // connection should continue or pause until a later call to
  // `continueResponse`.
  virtual FilterTrailersStatus onResponseTrailers(uint32_t /* trailers */) {
    return FilterTrailersStatus::Continue;
  }

  // Callback invoked when the stream has completed.
  virtual void onDone() {}

private:
  // For stream contexts, onDone always returns true.
  bool onDoneBase() override {
    onDone();
    return true;
  }

  RootContext *root_{};
};

// Returns stream context object associated with `context_id`, or nullptr if
// none exists (e.g. the stream has been destroyed, or the context associated
// with `context_id` is not a stream context).
Context *getContext(uint32_t context_id);

// Returns root context object associated with `context_id`, or nullptr if none
// exists (e.g. the context associated with `context_id` is not a root context).
RootContext *getRootContext(uint32_t context_id);

// Returns stream or root context object associated with `context_id`, or
// nullptr if none exists.
ContextBase *getContextBase(uint32_t context_id);

// Factory function type for root contexts.
using RootFactory =
    std::function<std::unique_ptr<RootContext>(uint32_t id, std::string_view root_id)>;

// Factory function type for stream contexts.
using ContextFactory = std::function<std::unique_ptr<Context>(uint32_t id, RootContext *root)>;

// Convenience macro to create a root context factory for the `RootContext`
// subclass named by `_c`.
#define ROOT_FACTORY(_c)                                                                           \
  [](uint32_t id, std::string_view root_id) -> std::unique_ptr<RootContext> {                      \
    return std::make_unique<_c>(id, root_id);                                                      \
  }

// Convenience macro to create a stream context factory for the `RootContext`
// subclass named by `_c`.
#define CONTEXT_FACTORY(_c)                                                                        \
  [](uint32_t id, RootContext *root) -> std::unique_ptr<Context> {                                 \
    return std::make_unique<_c>(id, root);                                                         \
  }

// Struct to instantiate in order to register root context and/or stream context
// factory functions.
struct RegisterContextFactory {
  // Registers `context_factory` for creating stream contexts and `root_factory`
  // for creating a root context for the plugin indicated by `root_id`.
  RegisterContextFactory(ContextFactory context_factory, RootFactory root_factory,
                         std::string_view root_id = "");

  // Registers `root_factory` for creating a root context for the plugin
  // indicated by `root_id`.
  explicit RegisterContextFactory(RootFactory root_factory, std::string_view root_id = "")
      : RegisterContextFactory(nullptr, root_factory, root_id) {}

  // Registers `context_factory` for creating stream contexts for the plugin
  // indicated by `root_id`.
  explicit RegisterContextFactory(ContextFactory context_factory, std::string_view root_id = "")
      : RegisterContextFactory(context_factory, nullptr, root_id) {}
};

// Returns the status code and status message of the response to an outbound
// HTTP or gRPC call. Can be called from the `RootContext::onHttpCallResponse`
// or `RootContext::onGrpcClose` callbacks.
inline std::pair<uint32_t, WasmDataPtr> getStatus() {
  uint32_t code = 0;
  const char *value_ptr = nullptr;
  size_t value_size = 0;
  CHECK_RESULT(proxy_get_status(&code, &value_ptr, &value_size));
  return std::make_pair(code, std::make_unique<WasmData>(value_ptr, value_size));
}

// Returns value of the property named by the concatenation of `parts`, or
// nullopt if no property value is found.
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

// Returns value of the property named by the concatenation of `parts`, or
// nullopt if no property value is found.
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

// Returns value of the property named by the concatenation of `parts` in
// out-param `out`, which can be of type int64, uint64, double, or bool. Returns
// true if the property was successfully fetched and stored in `out`, or false
// otherwise.
//
// Durations are represented as int64 nanoseconds. Timestamps are represented as
// int64 Unix nanoseconds.
template <typename T>
inline bool getValue(const std::initializer_list<std::string_view> &parts, T *out) {
  auto buf = getProperty(parts);
  if (!buf.has_value() || buf.value()->size() != sizeof(T)) {
    return false;
  }
  *out = *reinterpret_cast<const T *>(buf.value()->data());
  return true;
}

// Returns value of the property named by the concatenation of `parts` in string
// out-param `out`. Returns true if the property was successfully fetched and
// stored in `out`, or false otherwise.
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

// Returns value of the property named by the concatenation of `parts` in
// out-param `out`, which can be of type int64, uint64, double, or bool. Returns
// true if the property was successfully fetched and stored in `out`, or false
// otherwise.
//
// Durations are represented as int64 nanoseconds. Timestamps are represented as
// int64 Unix nanoseconds.
template <typename S, typename T> inline bool getValue(const std::vector<S> &parts, T *out) {
  auto buf = getProperty(parts);
  if (!buf.has_value() || buf.value()->size() != sizeof(T)) {
    return false;
  }
  *out = *reinterpret_cast<const T *>(buf.value()->data());
  return true;
}

// Returns value of the property named by the concatenation of `parts` in string
// out-param `out`. Returns true if the property was successfully fetched and
// stored in `out`, or false otherwise.
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

// Returns value of the property named by the concatenation of `parts` in string
// out-param `out`. Returns true if the property was successfully fetched and
// stored in `out`, or false otherwise.
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

// Returns value of the property named by the concatenation of `parts` in
// protobuf message out-param `out`. Returns true if the property was
// successfully fetched and stored in `out`, or false otherwise.
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

// Returns value of the property named by the concatenation of `parts` in
// protobuf message out-param `out`. Returns true if the property was
// successfully fetched and stored in `out`, or false otherwise.
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

// Sets property named by `key` to value `value`. Returns `WasmResult::Ok` on
// success, or `WasmResult::NotFound` if no such property exists.
inline WasmResult setFilterState(std::string_view key, std::string_view value) {
  return static_cast<WasmResult>(
      proxy_set_property(key.data(), key.size(), value.data(), value.size()));
}

// Sets property named by `key` to string value `s`. Returns `WasmResult::Ok` on
// success, or `WasmResult::NotFound` if no such property exists.
inline WasmResult setFilterStateStringValue(std::string_view key, std::string_view s) {
  return setFilterState(key, s);
}

// Resumes processing of a TCP(-like) connection after a previous callback to
// `Context::onDownstreamData` returned `FilterStatus::StopIteration`.
inline WasmResult continueDownstream() { return proxy_continue_stream(WasmStreamType::Downstream); }

// Resumes processing of paused TCP(-like) connection after a previous callback
// to `Context::onUpstreamData` returned `FilterStatus::StopIteration`.
inline WasmResult continueUpstream() { return proxy_continue_stream(WasmStreamType::Upstream); }

// Closes TCP(-like) connection in the downstream direction.
inline WasmResult closeDownstream() { return proxy_close_stream(WasmStreamType::Downstream); }

// Closes TCP(-like) connection in the upstream direction.
inline WasmResult closeUpstream() { return proxy_close_stream(WasmStreamType::Upstream); }

// Resumes processing of an HTTP request after a previous callback to
// `Context::onRequestHeaders`, `Context::onRequestBody`, or
// `Context::onRequestTrailers` returned a `Filter*Status` value that paused
// request processing.
inline WasmResult continueRequest() { return proxy_continue_stream(WasmStreamType::Request); }

// Resumes processing of an HTTP response after a previous callback to
// `Context::onResponseHeaders`, `Context::onResponseBody`, or
// `Context::onResponseTrailers` returned a `Filter*Status` value that paused
// response processing.
inline WasmResult continueResponse() { return proxy_continue_stream(WasmStreamType::Response); }

// Terminates processing of an HTTP request.
inline WasmResult closeRequest() { return proxy_close_stream(WasmStreamType::Request); }

// Terminates processing of an HTTP response.
inline WasmResult closeResponse() { return proxy_close_stream(WasmStreamType::Response); }

// Sends an HTTP response. `body` is the response body, `response_code` and
// `response_code_details` specify the response code and details to send,
// `additional_response_headers` gives response headers to send, and
// `grpc_status` specifies a gRPC status to return, if responding to an HTTP
// request that is gRPC.
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

// Fetches shared value identified by `key`, storing in out-param `value`. If
// `cas` is non-null, it is set to a compare-and-swap value that can be passed
// in a subsequent call to `setSharedData`, for atomic updates. Returns
// `WasmResult::Ok` if the value was successfully fetched, or
// `WasmResult::NotFound` if there is no shared data value for `key`.
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

// Sets shared data identified by `key` to value `value` if `cas` is 0 or
// matches the host's current compare-and-swap value for the entry. Returns
// `WasmResult::Ok` if the value was successfully set.
inline WasmResult setSharedData(std::string_view key, std::string_view value, uint32_t cas = 0) {
  return proxy_set_shared_data(key.data(), key.size(), value.data(), value.size(), cas);
}

// Returns shared value identified by `key`. If there is no shared data value
// for `key`, aborts the plugin by calling `logAbort`.
inline WasmDataPtr getSharedDataValue(std::string_view key, uint32_t *cas = nullptr) {
  WasmDataPtr data;
  auto result = getSharedData(key, &data, cas);
  if (result != WasmResult::Ok) {
    logAbort("getSharedData returned WasmError: " + toString(result));
  }
  return data;
}

// Registers a shared queue under the name `queue_name`, setting out-param
// `token` to a value that can be used to enqueue or dequeue items from the
// shared queue. If there is already a shared queue registered under
// `queue_name`, the call opens the existing queue. Returns `WasmResult::Ok` on
// success.
inline WasmResult registerSharedQueue(std::string_view queue_name, uint32_t *token) {
  return proxy_register_shared_queue(queue_name.data(), queue_name.size(), token);
}

// Resolves an existing shared queue under the name `queue_name` with VM ID
// `vm_id`, setting out-param `token` to a value that can be used to enqueue or
// dequeue items from the shared queue. Returns `WasmResult::Ok` on success.
inline WasmResult resolveSharedQueue(std::string_view vm_id, std::string_view queue_name,
                                     uint32_t *token) {
  return proxy_resolve_shared_queue(vm_id.data(), vm_id.size(), queue_name.data(),
                                    queue_name.size(), token);
}

// Enqueues `data` on the shared queue indicated by `token`. Returns
// `WasmResult::Ok` on success.
inline WasmResult enqueueSharedQueue(uint32_t token, std::string_view data) {
  return proxy_enqueue_shared_queue(token, data.data(), data.size());
}

// Dequeues an item from the shared queue indicated by `token`, storing it in
// out-param `data`. Returns `WasmResult::Ok` on success, or `WasmResult::Empty`
// if there is no item available to dequeue.
inline WasmResult dequeueSharedQueue(uint32_t token, WasmDataPtr *data) {
  const char *data_ptr = nullptr;
  size_t data_size = 0;
  auto result = proxy_dequeue_shared_queue(token, &data_ptr, &data_size);
  *data = std::make_unique<WasmData>(data_ptr, data_size);
  return result;
}

// Adds header field with name `key` and value `value` to the header map
// indicated by `type`. Returns `WasmResult::Ok` on success.
inline WasmResult addHeaderMapValue(WasmHeaderMapType type, std::string_view key,
                                    std::string_view value) {
  return proxy_add_header_map_value(type, key.data(), key.size(), value.data(), value.size());
}

// Returns value for the header field named by `key` in the header map indicated
// by `type`, or an empty `WasmData` if no such field is present in the header
// map.
inline WasmDataPtr getHeaderMapValue(WasmHeaderMapType type, std::string_view key) {
  const char *value_ptr = nullptr;
  size_t value_size = 0;
  proxy_get_header_map_value(type, key.data(), key.size(), &value_ptr, &value_size);
  return std::make_unique<WasmData>(value_ptr, value_size);
}

// Replaces header field with name `key` in the header map indicated by `type,
// setting its value to `value`, or adds the header field if it was not
// previously present. Returns `WasmResult::Ok` on success.
inline WasmResult replaceHeaderMapValue(WasmHeaderMapType type, std::string_view key,
                                        std::string_view value) {
  return proxy_replace_header_map_value(type, key.data(), key.size(), value.data(), value.size());
}

// Removes header field with name `key` from the header map indicated by
// `type. Returns `WasmResult::Ok` on success, which includes the case where no
// such header field was present in the header map.
inline WasmResult removeHeaderMapValue(WasmHeaderMapType type, std::string_view key) {
  return proxy_remove_header_map_value(type, key.data(), key.size());
}

// Returns all header fields for the header map indicated by `type`. Header
// fields can be accessed by calling `WasmData::pairs` on the return value.
inline WasmDataPtr getHeaderMapPairs(WasmHeaderMapType type) {
  const char *ptr = nullptr;
  size_t size = 0;
  proxy_get_header_map_pairs(type, &ptr, &size);
  return std::make_unique<WasmData>(ptr, size);
}

// Sets all header fields for the header map indicated by `type` to the entries
// contained in `pairs`. Returns `WasmResult::Ok` on success.
inline WasmResult setHeaderMapPairs(WasmHeaderMapType type, const HeaderStringPairs &pairs) {
  const char *ptr = nullptr;
  size_t size = 0;
  exportPairs(pairs, &ptr, &size);
  auto result = proxy_set_header_map_pairs(type, ptr, size);
  ::free(const_cast<char *>(ptr));
  return result;
}

// Fetches the total size in bytes of all header fields in the header map
// indicated by `type`, storing the result in out-param `size`. Returns
// `WasmResult::Ok` on success.
inline WasmResult getHeaderMapSize(WasmHeaderMapType type, size_t *size) {
  return proxy_get_header_map_size(type, size);
}

// Convenience functions for `*HeaderMap*` hostcalls with
// `WasmHeaderMapType::RequestHeaders`.
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

// Convenience functions for `*HeaderMap*` hostcalls with
// `WasmHeaderMapType::RequestTrailers`.
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

// Convenience functions for `*HeaderMap*` hostcalls with
// `WasmHeaderMapType::ResponseHeaders`.
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

// Convenience functions for `*HeaderMap*` hostcalls with
// `WasmHeaderMapType::ResponseTrailers`.
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

// Returns up to `length` bytes of data starting at offset `start` from the
// buffer of type `type`. Returns an empty `WasmData` on error.
inline WasmDataPtr getBufferBytes(WasmBufferType type, size_t start, size_t length) {
  const char *ptr = nullptr;
  size_t size = 0;
  proxy_get_buffer_bytes(type, start, length, &ptr, &size);
  return std::make_unique<WasmData>(ptr, size);
}

// Returns the number of bytes available in the buffer of type `type`, storing
// the result in out-param `size`. `flags` is currently ignored. Returns
// `WasmResult::Ok` on success, or `WasmResult::NotFound` if the indicated
// buffer is not currently accessible.
inline WasmResult getBufferStatus(WasmBufferType type, size_t *size, uint32_t *flags) {
  return proxy_get_buffer_status(type, size, flags);
}

// Sets `length` bytes from `data` into the buffer of type `type`, starting at
// offset `start` in the buffer.
inline WasmResult setBuffer(WasmBufferType type, size_t start, size_t length, std::string_view data,
                            size_t *new_size = nullptr) {
  auto result = proxy_set_buffer_bytes(type, start, length, data.data(), data.size());
  if (result == WasmResult::Ok && new_size)
    *new_size = *new_size - length + data.size();
  return result;
}

// Marshals HTTP header fields in `headers` into a newly allocated memory
// buffer. Returns a pointer to the buffer in out-param `buffer_ptr`, and the
// size of the buffer in out-param `size_ptr`.
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

// Initiates an outbound HTTP call to URI `uri`, sending `request_headers`,
// `request_body`, and `request_trailers` as the request headers, body, and
// trailers. Sets out-param `token_ptr` to a token value that will be passed to
// a corresponding callback to `RootContext::onHttpCallResponse`. Returns
// `WasmResult::Ok` if the request is successfully sent.
//
// This is a low-level API that is less ergonomic than `RootContext::httpCall`,
// which most plugin code should use instead.
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

// Defines a metric of type `type` named by `name`, storing in out-param
// `metric_id` an ID that can be used to set, increment, or fetch the metric
// value. Returns `WasmResult::Ok` on success.
//
// This is a low-level API; most plugin code should use the higher-level
// `Counter`, `Gauge`, or `Histogram` metrics utility classes.
inline WasmResult defineMetric(MetricType type, std::string_view name, uint32_t *metric_id) {
  return proxy_define_metric(type, name.data(), name.size(), metric_id);
}

// Increments the metric indicated by `metric_id` by `offset`. Returns
// `WasmResult::Ok` on success.
//
// This is a low-level API; most plugin code should use the higher-level
// `Counter`, `Gauge`, or `Histogram` metrics utility classes.
inline WasmResult incrementMetric(uint32_t metric_id, int64_t offset) {
  return proxy_increment_metric(metric_id, offset);
}

// Records measurement `value` to the metric indicated by `metric_id`. Returns
// `WasmResult::Ok` on success.
//
// This is a low-level API; most plugin code should use the higher-level
// `Counter`, `Gauge`, or `Histogram` metrics utility classes.
inline WasmResult recordMetric(uint32_t metric_id, uint64_t value) {
  return proxy_record_metric(metric_id, value);
}

// Stores the current value of the metric indicated by `metric_id` in out-param
// `value`. Returns `WasmResult::Ok` on success.
//
// This is a low-level API; most plugin code should use the higher-level
// `Counter`, `Gauge`, or `Histogram` metrics utility classes.
inline WasmResult getMetric(uint32_t metric_id, uint64_t *value) {
  return proxy_get_metric(metric_id, value);
}

// Tag that distinguishes one time series of values for a given metric from
// others for the same metric.
struct MetricTag {
  enum class TagType : uint32_t {
    String = 0,
    Int = 1,
    Bool = 2,
  };
  std::string name;
  TagType tagType;
};

// Base class for objects that represent metrics, which provides methods for
// defining metrics.
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

// Base class for objects that represent metrics, which provides methods for
// updating metric values.
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

// Object representing a single counter metric.
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

// Object representing a single gauge metric.
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

// Object representing a single histogram metric.
struct SimpleHistogram {
  SimpleHistogram(uint32_t id) : metric_id(id) {}

  void record(int64_t offset) { recordMetric(metric_id, offset); }

  uint32_t metric_id;
};

// Counter metric with support for tags. Each unique combination of tag values
// resolves to an independently updatable SimpleCounter.
template <typename... Tags> struct Counter : public MetricBase {
  static Counter<Tags...> *New(std::string_view name, MetricTagDescriptor<Tags>... fieldnames);

  template <typename... T>
  Counter(std::string_view name, MetricTagDescriptor<T>... descriptors)
      : Counter<T...>(std::string(name), std::vector<MetricTag>({toMetricTag(descriptors)...})) {}

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

// Gauge metric with support for tags. Each unique combination of tag values
// resolves to an independently updatable SimpleGauge.
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
      : Histogram<T...>(std::string(name), std::vector<MetricTag>({toMetricTag(descriptors)...})) {}

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

// Histogram metric with support for tags. Each unique combination of tag values
// resolves to an independently updatable SimpleHistogram.
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
