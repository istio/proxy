# Proxy-Wasm C++ SDK API Overview

This page explains the main concepts and structure of the C++ SDK API. For
detailed API semantics, refer to the doc comments in the header files listed in
the [Codemap] section, in particular [proxy_wasm_api.h].

## Concepts and terminology

* **Plugin**: a unit of code for customizing network proxy
  functionality. Plugins can be written in a variety of programming languages
  (in this case, C++), and are compiled to Wasm modules.
  
* **Wasm VM**: the virtual machine that runs plugins. A network proxy may
  internally instantiate multiple Wasm VMs to run plugins, for example one per
  worker thread. A given Wasm VM can host multiple plugins at the same time.
  
* **Hostcalls and callbacks**: *callbacks* are the entry points to the plugin:
  they are functions defined by the plugin that the proxy invokes when events of
  interest (e.g. plugin startup, receipt of a new incoming HTTP request)
  occur. *Hostcalls* are function calls that the plugin can make to the host
  (the network proxy) to query additional state or perform actions, such as
  reading and modifying HTTP response headers.
  
* **Contexts**: objects that are associated with the current plugin, or the
  current "stream" (either HTTP/gRPC request or, if L4 proxying, TCP stream). In
  the C++ SDK, Contexts serve multiple purposes:
  
  * They are the receivers of Proxy-Wasm callbacks: for example, when HTTP
    request headers are received, that is represented as a call to the
    `onRequestHeaders` method of a Context object. Callbacks are declared as
    virtual methods on a `Context` base class, which plugin code can override.
  * They provide the interface for most hostcalls, particularly those associated
    with the current stream. The hostcalls are defined as methods on `Context`
    base classes, which plugin code can call.
    
  There are two types of Contexts: *Plugin Contexts* (also known as Root
  Contexts) and *Stream Contexts* (sometimes just called Contexts). Plugin
  Contexts are represented by the `RootContext` class, and are associated with
  plugins as a whole--i.e. within a given Wasm VM, there is one Plugin Context
  per plugin. Stream Contexts are represented by the `Context` class, and are
  associated with individual streams--i.e. incoming HTTP/gRPC calls, or TCP
  streams.
  
  In the C++ SDK programming model, plugins are implemented by subclassing
  `RootContext` and/or `Context`, and providing implementations of their various
  callback methods. Plugins can also define fields in these subclasses to keep
  per-plugin or per-request state. A plugin's `RootContext` object lives for the
  full duration that the plugin runs in its Wasm VM, while each stream `Context`
  object lives for the duration of the HTTP/gRPC request with which it is
  associated.
  
## Registering Contexts

`RegisterContextFactory` is the mechanism for bootstrapping plugin code. Plugin
code instantiates a `RegisterContextFactory` in a static
variable. `RegisterContextFactory` takes as constructor params `std::function`s
for creating new plugin context and stream context instances. The plugin can use
these to create and return instances of its own subclasses of `RootContext` and
`Context`. For example:

```c++
static RegisterContextFactory register_ExampleContext(
    CONTEXT_FACTORY(ExampleContext), ROOT_FACTORY(ExampleRootContext));
```

`ROOT_FACTORY` and `CONTEXT_FACTORY` are convenience macros for lambdas that
create instances of the specified `RootContext` and `Context` subclasses.
        
## `RootContext`

`RootContext` instances are associated with the plugin as a whole. Accordingly,
they receive callbacks corresponding to different events in the lifecycle of a
plugin:

* `validateConfiguration`: May be called to validate the configuration data that
  will be passed to a plugin on startup, e.g. in the control plane.
* `onCreate`: called when context is created.
* `onStart`: called on plugin start.
* `onConfigure`: called on plugin start, and any time configuration subsequently
  changes.
* `onDone`: called when the plugin is being shut down.
* `onDelete`: called after the plugin has acknowledged shutdown is complete, as
  indication to release resources.

`RootContexts` can also receive other callbacks for events that are not strictly
tied to an incoming stream:

* `onTick`: called when a timer fires. See [Timers].
* `onQueueReady`: called when data arrives on a SharedQueue. See [Shared
  queues].

`RootContext` defines methods for initiating outbound HTTP and gRPC callouts:

* `httpCall`: initiates an HTTP callout (i.e. outbound HTTP request). See [HTTP
  callouts].
* `grpcSimpleCall`: initiates a gRPC callout (i.e. outbound gRPC call). See
  [gRPC callouts].
* `grpcCallHandler`: initiates a gRPC callout (i.e. outbound gRPC call). See
  [gRPC callouts].
* `grpcStreamHandler`: initiates a streaming gRPC callout (i.e. outbound gRPC
  call). See [gRPC callouts].

For API details, see doc comments for the `RootContext` class in
[proxy_wasm_api.h].

## `Context`

`Context` instances are associated with individual streams--i.e. incoming HTTP
or gRPC requests, or TCP streams if the network proxy is acting as an L4
proxy. `Context` instances receive callbacks corresponding to different events
in the lifecycle of a stream:

* `onCreate`: called when handling of a new stream starts.
* `onDone`: called when the host is done processing the stream.
* `onLog`: called after the host is done processing the stream, if the plugin is
  being used for access logging (for example, in Envoy, as a [WasmAccessLog]
  extension).
* `onDelete`: called after the plugin has completed all processing related to
  the stream, as indication to release resources.

`Context` instances also receive callbacks corresponding to stream events. For
HTTP or gRPC streams, these are:

* `onRequestHeaders`: called when HTTP or gRPC request headers are received.
* `onRequestBody`: called when a new chunk of HTTP or gRPC request body data is
  received.
* `onRequestTrailers`: called when HTTP or gRPC request trailers are received.
* `onResponseHeaders`: called when HTTP or gRPC response headers are received.
* `onResponseBody`: called when a new chunk of HTTP or gRPC response body data
  is received.
* `onResponseTrailers`: called when HTTP or gRPC response trailers are received.

For TCP streams, `Context` instances may receive the following callbacks:

* `onNewConnection`: called when a new connection is established.
* `onDownstreamData`: called when a new chunk of data is received from
  downstream over a connection.
* `onUpstreamData`: called when a new chunk of data is received from upstream
  over a connection.

Callback methods return status enums that indicate whether and how the host
should continue to process the stream. Status enum meanings are specified in the
doc comments in [proxy_wasm_enums.h]. Callbacks can also generate an immediate
local response to an HTTP or gRPC request using the `sendLocalResponse`
hostcall.

For API details, see doc comments for the `Context` class in [proxy_wasm_api.h].

## Buffers

Many plugin operations require passing data buffers between the host and the
plugin. Because this incurs some expense in copying data into or out of the Wasm
VM's linear address space, data is only fetched or passed when explicitly
requested via one of the following hostcalls:

* `getBufferBytes`: retrieves bytes from a given buffer.
* `getBufferStatus`: returns whether a given buffer is available, and if
  so, what its size is
* `setBuffer`: writes bytes into a given buffer.

With each of these calls, buffer data is held by an instance of the `WasmData`
class, which represents a span of bytes. The buffer to use as a source or
destination of data is indicated by the `WasmBufferType` enum. For example, the
`WasmBufferType::HttpResponseBody` enum value indicates the buffer used for
passing or receiving HTTP response body data.

Certain buffer types are only accessible during certain callbacks. For example,
the `WasmBufferType::HttpResponseBody` buffer is only accessible during
`Context::onResponseBody` and `Context::onLog` callbacks. See doc comments for
individual callbacks in [proxy_wasm_api.h] for which buffers are accessible in
each callback. Documentation for the `WasmData` class is also in
[proxy_wasm_api.h].

## Handling HTTP and gRPC request headers and trailers

Plugins that handle HTTP and gRPC requests do so by overriding one or more of
the HTTP-related callback methods declared by the `Context` class (see the
[Context] section for a list). For example, a plugin that needs to modify HTTP
request headers can do so by overriding the `Context::onRequestHeader`
method. The subclass implementation of `onRequestHeader` can use hostcalls to
fetch and/or mutate HTTP headers.

Hostcalls for accessing and modifying HTTP headers are listed below:

* `addHeaderMapValue`: adds a header field.
* `getHeaderMapValue`: returns the current value of a header field.
* `replaceHeaderMapValue`: replaces an existing header field.
* `removeHeaderMapValue`: removes a header field.
* `getHeaderMapPairs`: returns all header fields.
* `setHeaderMapPairs`: sets all header fields.
* `getHeaderMapSize`: returns the current number of header fields.

Each of the above hostcalls takes a `WasmHeaderMapType` that indicates the type
of headers targeted by the operation, e.g. `RequestHeaders`, `ResponseTrailers`,
etc. For convenience, the C++ SDK also provides functions that are specific to a
particular header type. For example:

* `addRequestHeader`: adds a request header field.
* `getRequestHeader`: returns the value of a request header field.
* `replaceRequestHeader`: replaces a request header field.
* `removeRequestHeader`: removes a request header field.
* `getRequestHeaderPairs`: returns all request header fields.
* `setRequestHeaderPairs`: sets all request header fields.
* `getRequestHeaderSize`: returns the current number of request header fields.

A similar set of methods is provided for each of: request trailers, response
headers, and response trailers.

gRPC requests received by the proxy are dispatched to the plugin using the same
`Context` callback methods as HTTP requests. Plugin code can determine whether
an incoming request is gRPC in the same way that network proxies do: by checking
if the ":method" pseudoheader has value "POST" and the "content-type" header
starts with value "application/grpc".

Plugin callbacks can access the request URL via the ":method", ":scheme",
":authority", and ":path" pseudo-headers defined in [RFC 9113 section
8.3.1]. They can access HTTP response status via the ":status" pseudo-header
defined in [RFC 9113 section 8.3.2].

For API details, see doc comments accompanying the functions in
[proxy_wasm_api.h].

## Handling TCP streams

Plugins that handle TCP (or TCP-like) connections do so by overriding one or
more of the connection-related callback methods declared by the `Context` class
(see the [Context] section for a list). For example, a plugin that wanted to
inspect data sent by the upstream client could override the
`Context::onUpstreamData` method.

To access the actual data being proxied, plugin code would use the
buffer-related hostcalls described in [Buffers], specifying
`NetworkUpstreamData` as the `WasmBufferType`.

## Timers

A plugin can schedule a periodic timer by calling the low-level
`proxy_set_tick_period_milliseconds` hostcall directly. The host will then call
`onTick` on the `RootContext` object periodically at the specified interval.

## HTTP callouts

Plugins can initiate outbound HTTP requests by invoking
`RootContext::httpCall`. This method initiates an HTTP request, taking as a
parameter a `std::function` to invoke when the request completes (or fails).

`RootContext::httpCall` wraps a lower-level hostcall, `makeHttpCall`, that uses
token values to match HTTP request and corresponding response callback.

For details, see doc comments for these functions in [proxy_wasm_api.h].

## gRPC callouts

Plugins can initiate outbound gRPC requests by invoking one of the following
hostcalls:

* `grpcSimpleCall`: initiates a gRPC call, taking a `std::function` to invoke
  upon call response or failure.
* `grpcCallHandler`: initiates a gRPC call, taking a `GrpcCallHandlerBase`
  object to call as various parts of the gRPC response are received.
* `grpcStreamHandler`: initiates a streaming gRPC call, taking a
  `GrpcStreamHandlerBase` object to call as new response data arrives over the
  gRPC stream. The `GrpcStreamHandlerBase` object can also be used to send
  additional data over the streaming gRPC call.

These `RootContext` methods wrap lower-level hostcalls that use token values to
match gRPC events to a given gRPC call:

* `grpcCall`: initiates a non-streaming gRPC call.
* `grpcStream`: initiates a streaming gRPC call.

The gRPC hostcalls listed above use the following supporting types as
parameters:

* `GrpcCallHandlerBase`: base class for handler objects that receive
  callbacks on events for a non-streaming gRPC call.
* `GrpcCallHandler`: subclass of GrpcCallHandlerBase that is templated by
  protobuf message type.
* `GrpcStreamHandlerBase`: base class for handler objects that receive
  callbacks on events for a streaming gRPC call.
* `GrpcStreamHandler`: subclass of GrpcStreamHandlerBase that is templated by
  protobuf message type.

Plugin code can subclass the appropriate type and override event handling
methods to process gRPC call responses.

For details of the gRPC hostcalls and their associated types, see doc comments
for these functions and types in [proxy_wasm_api.h].

## Shared data

Proxy-Wasm includes a concept of a shared key-value store that can be accessed
by multiple plugin instances. The C++ SDK provides this access via the following
hostcalls:

* `getSharedData`: reads the current value associated with a given key,
  returning it via out-param.
* `getSharedDataValue`: reads the current value associated with a given key,
  returning it via return value.
* `setSharedData`: sets the value associated with a given key.

In addition to a key param that specifies which entry in the key-value store to
read or write, these methods also accept and return a CAS (compare-and-swap)
value that can be used for atomic updates. For details, see doc comments
accompanying the functions in [proxy_wasm_api.h].

## Shared queues

Proxy-Wasm includes a concept of shared queues that can be used for
communication between multiple plugin instances. Shared queues are FIFO queues
that can have multiple producers and consumers.

The following hostcalls operate on shared queues:

* `registerSharedQueue`: establishes a shared queue under a given name.
* `resolveSharedQueue`: resolves an existing shared queue to an ID that can be
  used for enqueueing or dequeueing items to/from the queue.
* `enqueueSharedQueue`: writes a chunk of data onto the shared queue.
* `dequeueSharedQueue`: consumes a chunk of data from the shared queue.

The `RootContext::onQueueReady` callback informs a plugin when there is data
that is available to be consumed from a shared queue.

For API details, see doc comments in [proxy_wasm_api.h].

## Logging

Plugins can emit log messages for the host proxy to incorporate into its own
logging or otherwise record. Each logging operation takes a log level and string
message. The following logging macros log messages at various log levels, in
increasing order of severity, with file and function name automatically
prepended to the log message:

* `LOG_TRACE`
* `LOG_DEBUG`
* `LOG_INFO`
* `LOG_WARN`
* `LOG_ERROR`
* `LOG_CRITICAL`

The macros above allow plugin execution to continue. There is also a logging
hostcall that terminates plugin execution:

* `logAbort`: logs at Critical level, then aborts the plugin

For API details, see associated doc comments in [proxy_wasm_api.h].

## Metrics

Plugins can register and update metrics, to be exported by the host proxy. Each
metric can be one of the following types, represented by the `MetricType` enum:

* `MetricType::Counter`: numeric value representing a cumulative count of some event, which
  monotonically increases over time.
* `MetricType::Gauge`: numeric value representing a current snapshot of some quantity, which
  may take arbitrary values over time.
* `MetricType::Histogram`: a bucketed distribution of values.

The following low-level hostcalls support defining and updating metrics:

* `defineMetric`: registers a metric under a given name.
* `incrementMetric`: increments a metric counter.
* `recordMetric`: sets a metric counter.
* `getMetric`: reads a metric counter.

On top of these base hostcalls, the C++ SDK provides convenience classes
for representing and updating metrics:

- `Counter`
- `Gauge`
- `Histogram`
- `Metric`: all of the above

Metrics can be subdivided by tags, using similar structure to [Envoy
statistics].

## Foreign function interface (FFI)

Hosts can make additional hostcalls available for plugins to call via the
foreign function interface. Plugin code can invoke such hostcalls via the
`proxy_call_foreign_function` hostcall defined in [proxy_wasm_externs.h], which
specifies the foreign hostcall to invoke by a string name.

Hosts can also invoke callbacks for events outside of those defined by the
Proxy-Wasm spec, which are dispatched to the plugin via calls to
`ContextBase::onForeignFunction`. These calls use an integer function ID to
identify the foreign callback to invoke.

With both foreign hostcalls and callbacks, the plugin and host must agree
out-of-band on what hostcalls/callbacks will be available. For API details, see
associated doc comments in [proxy_wasm_externs.h] and [proxy_wasm_api.h].

## Example

Some of the concepts and APIs described above are illustrated by the example
plugin code in [http_wasm_example.cc].

First, the example plugin defines subclasses of `RootContext` and `Context`
that override callbacks of interest:

```c++
class ExampleRootContext : public RootContext {
public:
  explicit ExampleRootContext(uint32_t id, std::string_view root_id) : RootContext(id, root_id) {}
  bool onStart(size_t) override;
  bool onConfigure(size_t) override;
  // ...
};

class ExampleContext : public Context {
public:
  explicit ExampleContext(uint32_t id, RootContext *root) : Context(id, root) {}
  void onCreate() override;
  FilterHeadersStatus onRequestHeaders(uint32_t headers, bool end_of_stream) override;
  // ...
};
```

`RegisterContextFactory` registers these classes for creation:

```c++
static RegisterContextFactory register_ExampleContext(CONTEXT_FACTORY(ExampleContext),
                                                ROOT_FACTORY(ExampleRootContext),
                                                "my_root_id");
```

The implementations of callback methods use hostcalls to access and
mutate request or response state:

```c++
FilterHeadersStatus ExampleContext::onResponseHeaders(uint32_t, bool) {
  LOG_DEBUG(std::string("onResponseHeaders ") + std::to_string(id()));
  auto result = getResponseHeaderPairs();
  auto pairs = result->pairs();
  LOG_INFO(std::string("headers: ") + std::to_string(pairs.size()));
  for (auto &p : pairs) {
    LOG_INFO(std::string(p.first) + std::string(" -> ") + std::string(p.second));
  }
  addResponseHeader("X-Wasm-custom", "FOO");
  replaceResponseHeader("content-type", "text/plain; charset=utf-8");
  removeResponseHeader("content-length");
  return FilterHeadersStatus::Continue;
}
```

## Codemap

The main files containing the Proxy-Wasm C++ SDK API and implementation are
listed below:

* [proxy_wasm_api.h]: main SDK API definition and implementation
* [proxy_wasm_externs.h]: declarations for ABI-level hostcalls and callbacks
* [proxy_wasm_common.h]: supporting types for the API
* [proxy_wasm_enums.h]: supporting enums for the API
* [proxy_wasm_intrinsics.js]: list of Proxy-Wasm ABI hostcalls, for use by
  [Emscripten]
* [proxy_wasm_intrinsics.proto]: protobuf types needed for gRPC calls
* [proxy_wasm_intrinsics.h]: combined header file that includes all other header
  files
* [proxy_wasm_intrinsics.cc]: implementation of dispatch from ABI-level
  Proxy-Wasm callbacks to [Context] and [RootContext] callback methods.

[Buffers]: #buffers
[Codemap]: #codemap
[Context]: #context
[Emscripten]: https://emscripten.org
[Envoy statistics]: https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/observability/statistics
[HTTP callouts]: #http-callouts
[RFC 9113 section 8.3.1]: https://datatracker.ietf.org/doc/html/rfc9113#section-8.3.1
[RFC 9113 section 8.3.2]: https://datatracker.ietf.org/doc/html/rfc9113#section-8.3.2
[RootContext]: #rootcontext
[Shared queues]: #shared-queues
[Timers]: #timers
[WasmAccessLog]: https://www.envoyproxy.io/docs/envoy/latest/api-v3/extensions/access_loggers/wasm/v3/wasm.proto#extensions-access-loggers-wasm-v3-wasmaccesslog
[gRPC callouts]: #grpc-callouts
[http_wasm_example.cc]: ../example/http_wasm_example.cc
[proxy_wasm_api.h]: ../proxy_wasm_api.h
[proxy_wasm_common.h]: ../proxy_wasm_common.h
[proxy_wasm_enums.h]: ../proxy_wasm_enums.h
[proxy_wasm_externs.h]: ../proxy_wasm_externs.h
[proxy_wasm_intrinsics.cc]: ../proxy_wasm_intrinsics.cc
[proxy_wasm_intrinsics.h]: ../proxy_wasm_intrinsics.h
[proxy_wasm_intrinsics.js]: ../proxy_wasm_intrinsics.js
[proxy_wasm_intrinsics.proto]: ../proxy_wasm_intrinsics.proto
