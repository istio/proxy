WebAssembly
===========

Overview
--------

TODO: add text about high level design principal and feature set.

This [video talk](https://youtu.be/XdWmm_mtVXI) is a great introduction
about architecture of WebAssembly integration.

Configuration
-------------

-   v2 API reference (TODO: add link)
-   This filter should be configured with name *envoy.wasm*.

Example
-------

An example C++ WASM filter could be found
[here](https://github.com/envoyproxy/envoy-wasm/tree/19b9fd9a22e27fcadf61a06bf6aac03b735418e6/examples/wasm).

To implement a WASM filter:

-   Implement a [root context class](https://github.com/envoyproxy/envoy-wasm/blob/e8bf3ab26069a387f47a483d619221a0c482cd13/examples/wasm/envoy_filter_http_wasm_example.cc#L7) which inherits [base root context class](https://github.com/envoyproxy/envoy-wasm/blob/e8bf3ab26069a387f47a483d619221a0c482cd13/api/wasm/cpp/proxy_wasm_impl.h#L288)
-   Implement a [stream context
    class](https://github.com/envoyproxy/envoy-wasm/blob/e8bf3ab26069a387f47a483d619221a0c482cd13/examples/wasm/envoy_filter_http_wasm_example.cc#L14)
    which inherits the [base context
    class](https://github.com/envoyproxy/envoy-wasm/blob/master/api/wasm/cpp/proxy_wasm_impl.h#L314).
-   Override [context API](#context-object-api) methods to handle corresponding initialization and stream events
    from host.
-   [Register](https://github.com/envoyproxy/envoy-wasm/blob/e8bf3ab26069a387f47a483d619221a0c482cd13/examples/wasm/envoy_filter_http_wasm_example.cc#L26) the root context and stream context.

Context object
--------------

WASM module is running in a stack-based virtual machine and its memory
is isolated from the host environment. All interactions between host and
WASM module are through functions and callbacks wrapped by context
object.

At bootstrap time, a root context is created. The root context
has the same lifetime as the VM/runtime instance and acts as a target
for any interactions which happen at initial setup. It is also used for
interactions that outlive a request.

At request time, a context with incremental is created for
each stream. Stream context has the same lifetime as the stream itself
and acts as a target for interactions that are local to that stream.

![image](/docs/wasm_context.svg)

Context object API
------------------

### onConfigure

``` {.sourceCode .cpp}
void onConfigure(std::unique_ptr<WasmData> configuration)
```

Called when host loads the WASM module. *configuration* is passed in
using [WasmData](#wasmdata). If the VM that
the module running in has not been configured, onConfigure is called
first with
VM config (TODO: add link),
then a second call will be invoked to pass in
module config (TODO: add link).
*onConfigure* will only be called in
[root context](#context-object).

If VM is [shared](#vm-sharing) by multiple
filters and has already been configured via other WASM filter in the
chain, onConfigure will only be called once with module config.

### onStart

``` {.sourceCode .cpp}
void onStart()
```

Called after finishing loading WASM module and before serving any stream
events. *onStart* will only be called in
[root context](#context-object).

The following methods are called in order during the lifetime of a
stream.

### onCreate

``` {.sourceCode .cpp}
void onCreate()
```

Called at the beginning of filter chain iteration. Indicates creation of
the new stream context.

### onRequestHeaders

``` {.sourceCode .cpp}
void onRequestHeaders()
```

Called when headers are decoded. Request Headers could be fetched and
manipulated by
[request header API](#addrequestheader)

Returns
[FilterHeadersStatus](https://github.com/envoyproxy/envoy/blob/5d3214d4d8e1d77937f0f1278d3ac816d9a3d888/include/envoy/http/filter.h#L27)
to determine how filter chain iteration proceeds.

### onRequestBody

``` {.sourceCode .cpp}
FilterDataStatus onRequestBody(size_t body_buffer_length, bool end_of_stream)
```

Called when request body is decoded. *body\_buffer\_length* is used to
indicate size of decoded request body. *end\_of\_stream* indicates if
this is the last data frame. Request body could be fetched by
[body API](#body-api).

Returns
[FilterDataStatus](https://github.com/envoyproxy/envoy/blob/5d3214d4d8e1d77937f0f1278d3ac816d9a3d888/include/envoy/http/filter.h#L66)
to determine how filter chain iteration proceeds.

### onRequestTrailers

``` {.sourceCode .cpp}
FilterTrailersStatus onRequestTrailers()
```

Called when request trailers are decoded. Request trailers could be
fetched and manipulated by
request [trailer API](#addrequesttrailer).

Returns
[FilterTrailerStatus](https://github.com/envoyproxy/envoy/blob/5d3214d4d8e1d77937f0f1278d3ac816d9a3d888/include/envoy/http/filter.h#L104)
to determine how filter chain iteration proceeds.

### onResponseHeaders

``` {.sourceCode .cpp}
void onResponseHeaders()
```

Called when headers are decoded. Response headers could be fetched and
manipulated by
[response header API](#addresponseheader).

Returns
[FilterHeadersStatus](https://github.com/envoyproxy/envoy/blob/5d3214d4d8e1d77937f0f1278d3ac816d9a3d888/include/envoy/http/filter.h#L27)
to determine how filter chain iteration proceeds.

### onResponseBody

``` {.sourceCode .cpp}
FilterDataStatus onResponseBody(size_t body_buffer_length, bool end_of_stream)
```

Called when response body is decoded. *body\_buffer\_length* is used to
indicate size of decoded response body. *end\_of\_stream* indicates if
this is the last data frame. Response body could be fetched by
[body API](#body-api).

Returns
[FilterDataStatus](https://github.com/envoyproxy/envoy/blob/5d3214d4d8e1d77937f0f1278d3ac816d9a3d888/include/envoy/http/filter.h#L66)
to determine how filter chain iteration proceeds.

### onResponseTrailers

``` {.sourceCode .cpp}
FilterTrailersStatus onResponseTrailers()
```

Called when response trailers are decoded. Response trailers could be
fetched and manipulated
response [trailer API](#addrequesttrailer).

Returns FilterTrailerStatus
[FilterTrailerStatus](https://github.com/envoyproxy/envoy/blob/5d3214d4d8e1d77937f0f1278d3ac816d9a3d888/include/envoy/http/filter.h#L104)
to determine how filter chain iteration proceeds.

### onDone

``` {.sourceCode .cpp}
void onDone()
```

Called after stream is ended or reset. All stream info will not be
changed any more and is safe for access logging.

> **note**
>
> This is called before [onLog](#onlog).

### onLog

``` {.sourceCode .cpp}
void onLog()
```

Called to log any stream info. Several types of stream info are
available from API: Request headers could be fetched by
[request header API](#addrequestheader)
Response headers could be fetched by
[response header API](#addresponseheader).
Response trailers could be fetched by
response [trailer API](#addrequesttrailer).
Streaminfo could be fetched by
[streaminfo API](#streaminfo-api).

> **note**
>
> This is called after [onDone])(#ondone).

### onDelete

``` {.sourceCode .cpp}
void onDelete()
```

Called after logging is done. This call indicates no more handler will
be called on the stream context and it is up for deconstruction, The
stream context needs to make sure all async events are cleaned up, such
as network calls, timers.

Root context object is used to handle timer event.

### onTick

``` {.sourceCode .cpp}
void onTick()
```

Called when a timer is set and fired. Timer could be set by
[setTickPeriodMilliseconds](#setTickPeriodMilliseconds).

The following methods on context object are supported.

### httpCall

``` {.sourceCode .cpp}
void httpCall(std::string_view cluster,
              const HeaderStringPairs& request_headers,
              std::string_view request_body,
              const HeaderStringPairs& request_trailers,
              uint32_t timeout_milliseconds,
              HttpCallCallback callback)
```

Makes an HTTP call to an upstream host.

*cluster* is a string which maps to a configured cluster manager
cluster. *request\_headers* is a vector of key/value pairs to send. Note
that the *:method*, *:path*, and *:authority* headers must be set.
*request\_body* is an optional string of body data to send. timeout is
an integer that specifies the call timeout in milliseconds.
*timeout\_milliseconds* is an unsigned integer as timeout period for the
http call in milliseconds. *callback* is the callback function to be
called when the HTTP request finishes.

> **note**
>
> If the call outlives the stream context, *httpCall* should be called
> within [root context](#context-object).

### grpcSimpleCall

``` {.sourceCode .cpp}
template<typename Response>
void grpcSimpleCall(std::string_view service,
                    std::string_view service_name,
                    std::string_view method_name,
                    const google::protobuf::MessageLite &request,
                    uint32_t timeout_milliseconds,
                    std::function<void(Response&& response)> success_callback,
                    std::function<void(GrpcStatus status, std::string_view error_message)> failure_callback)
```

Makes a unary gRPC call to an upstream host.

*service* is a serialized proto string of
[gRPC service](https://www.envoyproxy.io/docs/envoy/v1.11.0/api-v2/api/v2/core/grpc_service.proto#core-grpcservice) for gRPC client
initialization. *service\_name* and *method\_name* indicates the target
gRPC service and method name. *request* is a [lite proto
message](https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.message_lite)
that gRPC service accepts as request. *timeout\_milliseconds* is an
unsigned integer as timeout period for the gRPC call in milliseconds.
*success\_callback* is the callback function that will be called when
gRPC call succeeds. *response* is the returned message from gRPC
service. *failure\_callback* is the callback function that will be
invoked when gRPC call fails. *status* is the returned gRPC status code.
*error\_message* is detailed error message extracted from gRPC response.

> **note**
>
> if the call outlives the stream context, *grpcSimpleCall* should be
> called within
> [root context](#context-object).

### grpcCallHandler

``` {.sourceCode .cpp}
void grpcCallHandler(
    std::string_view service,
    std::string_view service_name,
    std::string_view method_name,
    const google::protobuf::MessageLite &request,
    uint32_t timeout_milliseconds,
    std::unique_ptr<GrpcCallHandlerBase> handler)
```

Makes a unary gRPC call to an upstream host.

Similar to
[grpcSimpleCall](#grpcsimplecall)
for gRPC client initialization, but uses
[GrpcCallHandler](#GrpcCallHandler-class) as
target for callback and fine grained control on the call.

### grpcStreamHandler

``` {.sourceCode .cpp}
void grpcStreamHandler(std::string_view service,
                       std::string_view service_name,
                       std::string_view method_name,
                       std::unique_ptr<GrpcStreamHandlerBase> handler)
```

Makes an gRPC stream to an upstream host.

*service* is a serialized proto string of
[gRPC service](https://www.envoyproxy.io/docs/envoy/v1.11.0/api-v2/api/v2/core/grpc_service.proto#core-grpcservice) for gRPC client
initialization. *service\_name* and *method\_name* indicates the target
gRPC service and method name. *handler*
[GrpcStreamHandler](#grpcstreamhandler-class)
is used to control the stream and as target for gRPC stream callbacks.

> **note**
>
> if the stream call outlives the per request context,
> *grpcStreamHandler* should be called within
> [root context](#context-object).

Application log API
-------------------

### log\*

``` {.sourceCode .cpp}
void LogTrace(const std::string& logMessage)
void LogDebug(const std::string& logMessage)
void LogInfo(const std::string& logMessage)
void LogWarn(const std::string& logMessage)
void LogError(const std::string& logMessage)
void LogCritical(const std::string& logMessage)
```

Logs a message using Envoy's application logging. *logMessage* is a
string to log.

Header API
----------

### addRequestHeader

``` {.sourceCode .cpp}
void addRequestHeader(std::string_view key, StringView value)
```

Adds a new request header with the key and value if header does not
exist, or append the value if header exists. This method is effective
only when called in
[onRequestHeader](#onrequestheaders).

### replaceRequestHeader

``` {.sourceCode .cpp}
void replaceRequestHeader(std::string_view key, StringView value)
```

Replaces the value of an existing request header with the given key, or
create a new request header with the key and value if not existing. This
method is effective only when called in
[onRequestHeader](#onrequestheaders).

### removeRequestHeader

``` {.sourceCode .cpp}
void removeRequestHeader(std::string_view key)
```

Removes request header with the given key. No-op if the request header
does not exist. This method is effective only when called in
[onRequestHeader](#onrequestheaders).

### setRequestHeaderPairs

``` {.sourceCode .cpp}
void setRequestHeaderPairs(const HeaderStringPairs &pairs)
```

Sets request headers with the given header pairs. For each header key
value pair, it acts the same way as replaceRequestHeader. This method is
effective only when called in
[onRequestHeader](#onrequestheaders).

### getRequestHeader

``` {.sourceCode .cpp}
WasmDataPtr getRequestHeader(std::string_view key)
```

Gets value of header with the given key. Returns empty string if header
does not exist. This method is effective only when called in
[onRequestHeader](#onrequestheaders)
and [onLog](#onlog)

Returns [WasmData](#wasmdata) pointer which
contains the header value data.

### getRequestHeaderPairs

``` {.sourceCode .cpp}
WasmDataPtr getRequestHeaderPairs()
```

Gets all header pairs. This method is effective only when called in
[onRequestHeader](#onrequestheaders)
and [onLog](#onlog)

Returns [WasmData](#wasmdata) pointer which
contains header pairs data.

### addResponseHeader

``` {.sourceCode .cpp}
void addResponseHeader(std::string_view key, StringView value)
```

Adds a new response header with the key and value if header does not
exist, or append the value if header exists. This method is effective
only when called in
[onResponseHeaders](#onresponseheaders).

### replaceResponseHeader

``` {.sourceCode .cpp}
void replaceResponseHeader(std::string_view key, StringView value)
```

Replaces the value of an existing response header with the given key, or
create a new response header with the key and value if not existing.
This method is effective only when called in
[onResponseHeaders](#onresponseheaders).

### removeResponseHeader

``` {.sourceCode .cpp}
void removeResponseHeader(std::string_view key)
```

Removes response header with the given key. No-op if the response header
does not exist. This method is effective only when called in
[onResponseHeaders](#onresponseheaders).

### setResponseHeaderPairs

``` {.sourceCode .cpp}
void setResponseHeaderPairs(const HeaderStringPairs &pairs)
```

Sets response headers with the given header pairs. For each header key
value pair, it acts the same way as replaceResponseHeader. This method
is effective only when called in
[onResponseHeaders](#onresponseheaders).

### getResponseHeader

``` {.sourceCode .cpp}
WasmDataPtr getResponseHeader(std::string_view key)
```

Gets value of header with the given key. Returns empty string if header
does not exist. This method is effective only when called in
[onResponseHeaders](#onresponseheaders)
and [onLog](#onlog)

Returns [WasmData](#wasmdata) pointer which
holds the header value.

### getResponseHeaderPairs

``` {.sourceCode .cpp}
WasmDataPtr getResponseHeaderPairs()
```

Gets all header pairs. This method is effective only when called in
[onResponseHeaders](#onresponseheaders)
and [onLog](#onlog)

Returns [WasmData](#wasmdata) pointer which
holds the header pairs.

### addRequestTrailer

``` {.sourceCode .cpp}
void addRequestTrailer(std::string_view key, StringView value)
```

Adds a new request trailer with the key and value if trailer does not
exist, or append the value if trailer exists. This method is effective
only when called in
[onRequestTrailers](#onrequesttrailers).

### replaceRequestTrailer

``` {.sourceCode .cpp}
void replaceRequestTrailer(std::string_view key, StringView value)
```

Replaces the value of an existing request trailer with the given key, or
create a new request trailer with the key and value if not existing.
This method is effective only when called in
[onRequestTrailers](#onrequesttrailers).

### removeRequestTrailer

``` {.sourceCode .cpp}
void removeRequestTrailer(std::string_view key)
```

Removes request trailer with the given key. No-op if the request trailer
does not exist. This method is effective only when called in
[onRequestTrailers](#onrequesttrailers).

### setRequestTrailerPairs

``` {.sourceCode .cpp}
void setRequestTrailerPairs(const HeaderStringPairs &pairs)
```

Sets request trailers with the given trailer pairs. For each trailer key
value pair,it acts the same way as replaceRequestHeader. This method is
effective only when called in
[onRequestTrailers](#onrequesttrailers).

### getRequestTrailer

``` {.sourceCode .cpp}
WasmDataPtr getRequestTrailer(std::string_view key)
```

Gets value of trailer with the given key. Returns empty string if
trailer does not exist. This method is effective only when called in
[onRequestTrailers](#onrequesttrailers).

Returns [WasmData](#wasmdata) pointer which
holds the trailer value.

### getRequestTrailerPairs

``` {.sourceCode .cpp}
WasmDataPtr getRequestTrailerPairs()
```

Gets all trailer pairs. This method is effective only when called in
[onRequestTrailers](#onrequesttrailers).

Returns [WasmData](#wasmdata) pointer which
holds the trailer pairs.

### addResponseTrailer

``` {.sourceCode .cpp}
void addResponseTrailer(std::string_view key, StringView value)
```

Adds a new response trailer with the key and value if trailer does not
exist, or append the value if trailer exists. This method is effective
only when called in
[onResponseTrailers](#onresponsetrailers).

### replaceResponseTrailer

``` {.sourceCode .cpp}
void replaceResponseTrailer(std::string_view key, StringView value)
```

Replaces the value of an existing response trailer with the given key,
or create a new response trailer with the key and value if not existing.
This method is effective only when called in
[onResponseTrailers](#onresponsetrailers).

### removeResponseTrailer

``` {.sourceCode .cpp}
void removeResponseTrailer(std::string_view key)
```

Removes response trailer with the given key. No-op if the response
trailer does not exist. This method is effective only when called in
[onResponseTrailers](#onresponsetrailers).

### setResponseTrailerPairs

``` {.sourceCode .cpp}
void setResponseTrailerPairs(const TrailerStringPairs &pairs)
```

Sets response trailers with the given trailer pairs. For each trailer
key value pair, it acts the same way as replaceResponseTrailer. This
method is effective only when called in
[onResponseTrailers](#onresponsetrailers).

### getResponseTrailer

``` {.sourceCode .cpp}
WasmDataPtr getResponseTrailer(std::string_view key)
```

Gets value of trailer with the given key. Returns empty string if
trailer does not exist. This method is effective only when called in
[onResponseTrailers](#onresponsetrailers)
and [onLog](#onlog)

Returns [WasmData](#wasmdata) pointer which
holds the trailer value.

### getResponseTrailerPairs

``` {.sourceCode .cpp}
WasmDataPtr getResponseTrailerPairs()
```

Gets all trailer pairs. This method is effective only when called in
[onResponseTrailers](#onresponsetrailers)
and [onLog](#onlog)

Returns [WasmData](#wasmdata) pointer which
holds the trailer pairs.

Body API
--------

### getRequestBodyBufferBytes

``` {.sourceCode .cpp}
WasmDataPtr getRequestBodyBufferBytes(size_t start, size_t length)
```

Returns buffered request body. This copies segment of request body.
*start* is an integer and supplies the body buffer start index to copy.
*length* is an integer and supplies the buffer length to copy. This
method is effective when calling from
[onRequestBody](#onrequestbody).

Returns [WasmData](#wasmdata) pointer which
holds the request body data.

### getResponseBodyBufferBytes

``` {.sourceCode .cpp}
WasmDataPtr getResponseBodyBufferBytes(size_t start, size_t length)
```

Returns buffered response body. This copies segment of response body.
*start* is an integer and supplies the body buffer start index to copy.
*length* is an integer and supplies the buffer length to copy. This
method is effective when calling from
[onResponseBody](#onresponsebody).

Returns [WasmData](#wasmdata) pointer which
holds the response body data.

Metadata API
------------

TODO: Add metadata related API

StreamInfo API
--------------

### getProtocol

``` {.sourceCode .cpp}
WasmDataPtr getProtocol(StreamType type)
```

Returns the string representation of HTTP protocol used by the current
request. The possible values are: HTTP/1.0, HTTP/1.1, and HTTP/2. *type*
is the stream type with two possible values: StreamType::Request and
StreamType::Response. The string protocol is returned as
[WasmData](#wasmdata).

Timer API
---------

Timer API is used to set a timer and get current timestamp.

### setTickPeriodMilliseconds

``` {.sourceCode .cpp}
void setTickPeriodMilliseconds(uint32_t millisecond)
```

Set a timer. *millisecond* is tick interval in millisecond.
[onTick](#ontick)
will be invoked when timer fires.

> **note**
>
> Only one timer could be set per root_id which is vectored to the appropriate RootContext. Any context can call setTickPeriodMilliseconds and the onTick will come on the corresponding RootContext.

### getCurrentTimeNanoseconds

``` {.sourceCode .cpp}
uint64 getCurrentTimeNanoseconds()
```

Returns timestamp of now in nanosecond precision.

Stats API
---------

The following objects are supported to export stats from WASM module to
host stats sink.

### Counter

#### New

``` {.sourceCode .cpp}
static Counter<Tags...>* New(std::string_view name, MetricTagDescriptor<Tags>... fieldnames)
```

Create a new counter with the given metric name and tag names. Example
code to create a counter metric:

``` {.sourceCode .cpp}
auto c = Counter<std::string, int, bool>::New(
             "test_counter", "string_tag", "int_tag", "bool_tag");
```

Returns a pointer to counter object.

### increment

``` {.sourceCode .cpp}
void increment(int64_t offset, Tags... tags)
```

Increments a counter. *offset* is the value the counter incremented by.
*tags* is a list of tag values to identify a specific counter. Example
code to increment the aforementioned counter:

``` {.sourceCode .cpp}
c->increment(1, "test_tag", 7, true)
```

### get

``` {.sourceCode .cpp}
uint64_t get(Tags... tags)
```

Returns value of a counter. *tags* is a list of tag values to identify a
specific counter. Example code to get value of a counter:

``` {.sourceCode .cpp}
c->get("test_tag", 7, true);
```

### resolve

``` {.sourceCode .cpp}
SimpleCounter resolve(Tags... f)
```

Resolves counter object to a specific counter for a list of tag values.

Returns a [SimpleCounter](#simplecounter)
resolved from the counter object, so that tag values do not need to be
specified in every increment call. Example code:

``` {.sourceCode .cpp}
auto simple_counter = c->resolve("test_tag", 7, true);
```

### SimpleCounter

*SimpleCounter* is resolved from a
[Counter](#counter) object with
predetermined tag values.

### increment

``` {.sourceCode .cpp}
void increment(int64_t offset)
```

Increment a counter. *offset* is the value counter incremented by.

### get

``` {.sourceCode .cpp}
uint64_t get()
```

Returns current value of a counter.

### Gauge

#### New

``` {.sourceCode .cpp}
static Gauge<Tags...>* New(std::string_view name, MetricTagDescriptor<Tags>... fieldnames)
```

Create a new gauge with the given metric name and tag names. Example
code to create a gauge metric:

``` {.sourceCode .cpp}
auto c = Gauge<std::string, int, bool>::New(
             "test_gauge", "string_tag", "int_tag", "bool_tag");
```

Returns a pointer to Gauge object.

### record

``` {.sourceCode .cpp}
void record(int64_t offset, Tags... tags)
```

Records current value of a gauge. *offset* is the value to set for
current gauge. *tags* is a list of tag values to identify a specific
gauge. Example code to record value of a gauge metric:

``` {.sourceCode .cpp}
c->record(1, "test_tag", 7, true)
```

### get

``` {.sourceCode .cpp}
uint64_t get(Tags... tags)
```

Returns value of a gauge. *tags* is a list of tag values to identify a
specific gauge. Example code to get value of a gauge:

``` {.sourceCode .cpp}
c->get("test_tag", 7, true);
```

### resolve

``` {.sourceCode .cpp}
SimpleGauge resolve(Tags... f)
```

Resolves gauge object to a specific gauge for a list of tag values.

Returns a [SimpleGauge](#SimpleGauge)
resolved from the gauge object, so that tag values do not need to be
specified in every record call. Example code:

``` {.sourceCode .cpp}
auto simple_gauge = c->resolve("test_tag", 7, true);
```

### SimpleGauge

*SimpleGauge* is resolved from a
[Gauge](#Gauge) object with predetermined
tag values.

### record

``` {.sourceCode .cpp}
void record(int64_t offset)
```

Records current value of a gauge. *offset* is the value to set for
current gauge.

### get

``` {.sourceCode .cpp}
uint64_t get()
```

Returns current value of a gauge.

### Histogram

#### New

``` {.sourceCode .cpp}
static Histogram<Tags...>* New(std::string_view name, MetricTagDescriptor<Tags>... fieldnames)
```

Create a new histogram object with the given metric name and tag names.
Example code to create a histogram metric:

``` {.sourceCode .cpp}
auto h = Histogram<std::string, int, bool>::New(
             "test_histogram", "string_tag", "int_tag", "bool_tag");
```

Returns a pointer to Histogram object.

### record

``` {.sourceCode .cpp}
void record(int64_t offset, Tags... tags)
```

Records a value in histogram stats. *offset* is the value to be
recorded. *tags* is a list of tag values to identify a specific
histogram. Example code to add a new value into histogram:

``` {.sourceCode .cpp}
h->record(1, "test_tag", 7, true)
```

### resolve

``` {.sourceCode .cpp}
SimpleHistogram resolve(Tags... f)
```

Resolves histogram object to a specific histogram for a list of tag
values.

Returns a
[SimpleHistogram](#SimpleHistogram)
resolved from the histogram object, so that tag values do not need to be
specified in every record call. Example code:

``` {.sourceCode .cpp}
auto simple_histogram = c->resolve("test_tag", 7, true);
```

### SimpleHistogram

*SimpleHistogram* is resolved from a
[Histogram](#Histogram) object with
predetermined tag values.

### record

``` {.sourceCode .cpp}
void record(int64_t offset)
```

Records a value in histogram. *offset* is the value to be recorded.

Data Structure
--------------

### GrpcCallHandler class

Base class for gRPC unary call handler. Subclass should specify response
message type and override necessary callbacks. Example code to create a
call handler using *google::protobuf::Empty* as response message.

``` {.sourceCode .cpp}
class CallHandler : public GrpcCallHandler<google::protobuf::Empty> {
  public:
    void onSuccess(google::protobuf::Empty&& response) {
        /* override onSuccess code */
    }
    /*
        more callbacks such as onFailure
    */
};
```

To initialize a handler, pass in a pointer to
[context object](#context-object) that
this call should attach to. For example, passing in root context:

``` {.sourceCode .cpp}
auto handler = std::make_unique<CallHandler>(&root_context);
```

Note the context object needs to outlive the call. *handler* is also
used for WASM module to interact with the stream, such as canceling the
call.

#### onSuccess

``` {.sourceCode .cpp}
void onSuccess(Message&& response)
```

Called when the async gRPC request succeeds. No further callbacks will
be invoked.

#### onFailure

``` {.sourceCode .cpp}
void onFailure(GrpcStatus status, std::unique_ptr<WasmData> error_message)
```

Called when the async gRPC request fails. No further callbacks will be
invoked. *status* is returned grpc status. *error\_message* is the gRPC
status message or empty string if not present.

#### cancel

``` {.sourceCode .cpp}
void cancel()
```

Signals that the request should be cancelled. No further callbacks will
be invoked.

### GrpcStreamHandler class

Base class for gRPC stream handler. Subclass should specify stream
message type and override callbacks. Example code to create a stream
handler using *google::protobuf::Struct* as request message and
*google::protobuf::Any* response message:

``` {.sourceCode .cpp}
class StreamHandler : public GrpcStreamHandler<google::protobuf::Struct, google::protobuf::Any> {
  public:
    void onReceive(google::protobuf::Any&& message) {
        /* override onReceive code */
    }
    /*
        more callbacks such as onReceiveTrailingMetadata, onReceive, onRemoteClose
    */
};
```

To initialize a handler, pass in a pointer to
[context object](#context-object) that
this stream should attach to. For example, passing in root context:

``` {.sourceCode .cpp}
auto handler = std::make_unique<StreamHandler>(&root_context);
```

Note the context object needs to outlive the stream. *handler* is also
used for WASM module to interact with the stream, such as sending
message, closing and resetting stream.

#### send

``` {.sourceCode .cpp}
void send(const Request& message, bool end_of_stream)
```

Sends a request message to the stream. *end\_of\_stream* indicates if
this is the last message to send. With *end\_of\_stream* as true,
callbacks can still occur.

#### close

``` {.sourceCode .cpp}
void close()
```

Close the stream locally and send an empty DATA frame to the remote. No
further methods may be invoked on the stream object, but callbacks may
still be received until the stream is closed remotely.

#### reset

``` {.sourceCode .cpp}
void reset()
```

Close the stream locally and remotely (as needed). No further methods
may be invoked on the handler object and no further callbacks will be
invoked.

#### onReceiveInitialMetadata

``` {.sourceCode .cpp}
void onReceiveInitialMetadata()
```

Called when initial metadata is received. This will be called with empty
metadata on a trailers-only response, followed by
onReceiveTrailingMetadata() with the trailing metadata. . TODO: how to
get initial metadata?

#### onReceiveTrailingMetadata

``` {.sourceCode .cpp}
void onReceiveTrailingMetadata()
```

Called when trailing metadata is received. This will also be called on
non-Ok grpc-status stream termination.

#### onReceive

``` {.sourceCode .cpp}
void onReceive(Response&& message)
```

Called when an async gRPC message is received.

#### onRemoteClose

``` {.sourceCode .cpp}
void onRemoteClose(GrpcStatus status, std::unique_ptr<WasmData> error_message)
```

Called when the remote closes or an error occurs on the gRPC stream. The
stream is considered remotely closed after this invocation and no
further callbacks will be invoked. In addition, no further stream
operations are permitted. *status* is the grpc status, *error\_message*
is the gRPC status error message or empty string if not present.

### WasmData

WasmData is used to represent data passed into WASM module from host. It
is like string view, which holds a pointer to start of the data and a
size. It also supports several methods to access the data.

#### data

``` {.sourceCode .cpp}
const char* data()
```

Returns the start pointer of the data.

#### view

``` {.sourceCode .cpp}
std::string_view view()
```

Returns data as a string view constructed with the start pointer and the
size.

#### toString

``` {.sourceCode .cpp}
std::string toString()
```

Returns data as a string by converting the string view to string.

#### pairs

``` {.sourceCode .cpp}
std::vector<std::pair<std::string_view, StringView>> pairs()
```

Returns a vector of string view pair parsed from the data.

#### proto

``` {.sourceCode .cpp}
template<typename T> T proto()
```

Returns a proto message parsed from the data based on the specified
proto type.

Out of tree WASM module
-----------------------

TODO: add an example about out of tree WASM module example

VM Sharing
----------

TODO: add instruction about vm sharing
