// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-js.h"

#include <cinttypes>
#include <cstring>

#include "include/v8-function.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-promise.h"
#include "include/v8-wasm.h"
#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/base/logging.h"
#include "src/execution/execution.h"
#include "src/execution/isolate.h"
#include "src/execution/messages.h"
#include "src/handles/handles.h"
#include "src/heap/factory.h"
#include "src/objects/fixed-array.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-function.h"
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/templates.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-serialization.h"
#include "src/wasm/wasm-value.h"

using v8::internal::wasm::ErrorThrower;
using v8::internal::wasm::ScheduledErrorThrower;

namespace v8 {

class WasmStreaming::WasmStreamingImpl {
 public:
  WasmStreamingImpl(
      Isolate* isolate, const char* api_method_name,
      std::shared_ptr<internal::wasm::CompilationResultResolver> resolver)
      : isolate_(isolate), resolver_(std::move(resolver)) {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate_);
    auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
    streaming_decoder_ = i::wasm::GetWasmEngine()->StartStreamingCompilation(
        i_isolate, enabled_features, handle(i_isolate->context(), i_isolate),
        api_method_name, resolver_);
  }

  void OnBytesReceived(const uint8_t* bytes, size_t size) {
    streaming_decoder_->OnBytesReceived(base::VectorOf(bytes, size));
  }
  void Finish(bool can_use_compiled_module) {
    streaming_decoder_->Finish(can_use_compiled_module);
  }

  void Abort(MaybeLocal<Value> exception) {
    i::HandleScope scope(reinterpret_cast<i::Isolate*>(isolate_));
    streaming_decoder_->Abort();

    // If no exception value is provided, we do not reject the promise. This can
    // happen when streaming compilation gets aborted when no script execution
    // is allowed anymore, e.g. when a browser tab gets refreshed.
    if (exception.IsEmpty()) return;

    resolver_->OnCompilationFailed(
        Utils::OpenHandle(*exception.ToLocalChecked()));
  }

  bool SetCompiledModuleBytes(base::Vector<const uint8_t> bytes) {
    if (!i::wasm::IsSupportedVersion(bytes)) return false;
    streaming_decoder_->SetCompiledModuleBytes(bytes);
    return true;
  }

  void SetMoreFunctionsCanBeSerializedCallback(
      std::function<void(CompiledWasmModule)> callback) {
    streaming_decoder_->SetMoreFunctionsCanBeSerializedCallback(
        [callback = std::move(callback),
         streaming_decoder = streaming_decoder_](
            const std::shared_ptr<i::wasm::NativeModule>& native_module) {
          base::Vector<const char> url = streaming_decoder->url();
          callback(CompiledWasmModule{native_module, url.begin(), url.size()});
        });
  }

  void SetUrl(base::Vector<const char> url) { streaming_decoder_->SetUrl(url); }

 private:
  Isolate* const isolate_;
  std::shared_ptr<internal::wasm::StreamingDecoder> streaming_decoder_;
  std::shared_ptr<internal::wasm::CompilationResultResolver> resolver_;
};

WasmStreaming::WasmStreaming(std::unique_ptr<WasmStreamingImpl> impl)
    : impl_(std::move(impl)) {
  TRACE_EVENT0("v8.wasm", "wasm.InitializeStreaming");
}

// The destructor is defined here because we have a unique_ptr with forward
// declaration.
WasmStreaming::~WasmStreaming() = default;

void WasmStreaming::OnBytesReceived(const uint8_t* bytes, size_t size) {
  TRACE_EVENT1("v8.wasm", "wasm.OnBytesReceived", "bytes", size);
  impl_->OnBytesReceived(bytes, size);
}

void WasmStreaming::Finish(bool can_use_compiled_module) {
  TRACE_EVENT0("v8.wasm", "wasm.FinishStreaming");
  impl_->Finish(can_use_compiled_module);
}

void WasmStreaming::Abort(MaybeLocal<Value> exception) {
  TRACE_EVENT0("v8.wasm", "wasm.AbortStreaming");
  impl_->Abort(exception);
}

bool WasmStreaming::SetCompiledModuleBytes(const uint8_t* bytes, size_t size) {
  TRACE_EVENT0("v8.wasm", "wasm.SetCompiledModuleBytes");
  return impl_->SetCompiledModuleBytes(base::VectorOf(bytes, size));
}

void WasmStreaming::SetMoreFunctionsCanBeSerializedCallback(
    std::function<void(CompiledWasmModule)> callback) {
  impl_->SetMoreFunctionsCanBeSerializedCallback(std::move(callback));
}

void WasmStreaming::SetUrl(const char* url, size_t length) {
  DCHECK_EQ('\0', url[length]);  // {url} is null-terminated.
  TRACE_EVENT1("v8.wasm", "wasm.SetUrl", "url", url);
  impl_->SetUrl(base::VectorOf(url, length));
}

// static
std::shared_ptr<WasmStreaming> WasmStreaming::Unpack(Isolate* isolate,
                                                     Local<Value> value) {
  TRACE_EVENT0("v8.wasm", "wasm.WasmStreaming.Unpack");
  i::HandleScope scope(reinterpret_cast<i::Isolate*>(isolate));
  auto managed =
      i::Handle<i::Managed<WasmStreaming>>::cast(Utils::OpenHandle(*value));
  return managed->get();
}

namespace {

#define ASSIGN(type, var, expr)                      \
  Local<type> var;                                   \
  do {                                               \
    if (!expr.ToLocal(&var)) {                       \
      DCHECK(i_isolate->has_scheduled_exception());  \
      return;                                        \
    } else {                                         \
      DCHECK(!i_isolate->has_scheduled_exception()); \
    }                                                \
  } while (false)

i::Handle<i::String> v8_str(i::Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromAsciiChecked(str);
}
Local<String> v8_str(Isolate* isolate, const char* str) {
  return Utils::ToLocal(v8_str(reinterpret_cast<i::Isolate*>(isolate), str));
}

#define GET_FIRST_ARGUMENT_AS(Type)                                  \
  i::MaybeHandle<i::Wasm##Type##Object> GetFirstArgumentAs##Type(    \
      const v8::FunctionCallbackInfo<v8::Value>& args,               \
      ErrorThrower* thrower) {                                       \
    i::Handle<i::Object> arg0 = Utils::OpenHandle(*args[0]);         \
    if (!arg0->IsWasm##Type##Object()) {                             \
      thrower->TypeError("Argument 0 must be a WebAssembly." #Type); \
      return {};                                                     \
    }                                                                \
    return i::Handle<i::Wasm##Type##Object>::cast(arg0);             \
  }

GET_FIRST_ARGUMENT_AS(Module)
GET_FIRST_ARGUMENT_AS(Tag)

#undef GET_FIRST_ARGUMENT_AS

i::wasm::ModuleWireBytes GetFirstArgumentAsBytes(
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower* thrower,
    bool* is_shared) {
  const uint8_t* start = nullptr;
  size_t length = 0;
  v8::Local<v8::Value> source = args[0];
  if (source->IsArrayBuffer()) {
    // A raw array buffer was passed.
    Local<ArrayBuffer> buffer = Local<ArrayBuffer>::Cast(source);
    auto backing_store = buffer->GetBackingStore();

    start = reinterpret_cast<const uint8_t*>(backing_store->Data());
    length = backing_store->ByteLength();
    *is_shared = buffer->IsSharedArrayBuffer();
  } else if (source->IsTypedArray()) {
    // A TypedArray was passed.
    Local<TypedArray> array = Local<TypedArray>::Cast(source);
    Local<ArrayBuffer> buffer = array->Buffer();

    auto backing_store = buffer->GetBackingStore();

    start = reinterpret_cast<const uint8_t*>(backing_store->Data()) +
            array->ByteOffset();
    length = array->ByteLength();
    *is_shared = buffer->IsSharedArrayBuffer();
  } else {
    thrower->TypeError("Argument 0 must be a buffer source");
  }
  DCHECK_IMPLIES(length, start != nullptr);
  if (length == 0) {
    thrower->CompileError("BufferSource argument is empty");
  }
  size_t max_length = i::wasm::max_module_size();
  if (length > max_length) {
    thrower->RangeError("buffer source exceeds maximum size of %zu (is %zu)",
                        max_length, length);
  }
  if (thrower->error()) return i::wasm::ModuleWireBytes(nullptr, nullptr);
  return i::wasm::ModuleWireBytes(start, start + length);
}

namespace {
i::MaybeHandle<i::JSReceiver> ImportsAsMaybeReceiver(Local<Value> ffi) {
  if (ffi->IsUndefined()) return {};

  Local<Object> obj = Local<Object>::Cast(ffi);
  return i::Handle<i::JSReceiver>::cast(v8::Utils::OpenHandle(*obj));
}

// This class resolves the result of WebAssembly.compile. It just places the
// compilation result in the supplied {promise}.
class AsyncCompilationResolver : public i::wasm::CompilationResultResolver {
 public:
  AsyncCompilationResolver(Isolate* isolate, Local<Context> context,
                           Local<Promise::Resolver> promise_resolver)
      : isolate_(isolate),
        context_(isolate, context),
        promise_resolver_(isolate, promise_resolver) {
    context_.SetWeak();
    promise_resolver_.AnnotateStrongRetainer(kGlobalPromiseHandle);
  }

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> result) override {
    if (finished_) return;
    finished_ = true;
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<i::Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(i::Handle<i::Object>::cast(result)),
             WasmAsyncSuccess::kSuccess);
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    if (finished_) return;
    finished_ = true;
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<i::Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(error_reason), WasmAsyncSuccess::kFail);
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "AsyncCompilationResolver::promise_";
  bool finished_ = false;
  Isolate* isolate_;
  Global<Context> context_;
  Global<Promise::Resolver> promise_resolver_;
};

constexpr char AsyncCompilationResolver::kGlobalPromiseHandle[];

// This class resolves the result of WebAssembly.instantiate(module, imports).
// It just places the instantiation result in the supplied {promise}.
class InstantiateModuleResultResolver
    : public i::wasm::InstantiationResultResolver {
 public:
  InstantiateModuleResultResolver(Isolate* isolate, Local<Context> context,
                                  Local<Promise::Resolver> promise_resolver)
      : isolate_(isolate),
        context_(isolate, context),
        promise_resolver_(isolate, promise_resolver) {
    context_.SetWeak();
    promise_resolver_.AnnotateStrongRetainer(kGlobalPromiseHandle);
  }

  void OnInstantiationSucceeded(
      i::Handle<i::WasmInstanceObject> instance) override {
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<i::Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(i::Handle<i::Object>::cast(instance)),
             WasmAsyncSuccess::kSuccess);
  }

  void OnInstantiationFailed(i::Handle<i::Object> error_reason) override {
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<i::Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(error_reason), WasmAsyncSuccess::kFail);
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "InstantiateModuleResultResolver::promise_";
  Isolate* isolate_;
  Global<Context> context_;
  Global<Promise::Resolver> promise_resolver_;
};

constexpr char InstantiateModuleResultResolver::kGlobalPromiseHandle[];

// This class resolves the result of WebAssembly.instantiate(bytes, imports).
// For that it creates a new {JSObject} which contains both the provided
// {WasmModuleObject} and the resulting {WebAssemblyInstanceObject} itself.
class InstantiateBytesResultResolver
    : public i::wasm::InstantiationResultResolver {
 public:
  InstantiateBytesResultResolver(Isolate* isolate, Local<Context> context,
                                 Local<Promise::Resolver> promise_resolver,
                                 Local<Value> module)
      : isolate_(isolate),
        context_(isolate, context),
        promise_resolver_(isolate, promise_resolver),
        module_(isolate, module) {
    context_.SetWeak();
    promise_resolver_.AnnotateStrongRetainer(kGlobalPromiseHandle);
    module_.AnnotateStrongRetainer(kGlobalModuleHandle);
  }

  void OnInstantiationSucceeded(
      i::Handle<i::WasmInstanceObject> instance) override {
    if (context_.IsEmpty()) return;
    Local<Context> context = context_.Get(isolate_);

    // The result is a JSObject with 2 fields which contain the
    // WasmInstanceObject and the WasmModuleObject.
    Local<Object> result = Object::New(isolate_);
    result
        ->CreateDataProperty(context, v8_str(isolate_, "module"),
                             module_.Get(isolate_))
        .Check();
    result
        ->CreateDataProperty(
            context, v8_str(isolate_, "instance"),
            Utils::ToLocal(i::Handle<i::Object>::cast(instance)))
        .Check();

    auto callback = reinterpret_cast<i::Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context, promise_resolver_.Get(isolate_), result,
             WasmAsyncSuccess::kSuccess);
  }

  void OnInstantiationFailed(i::Handle<i::Object> error_reason) override {
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<i::Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(error_reason), WasmAsyncSuccess::kFail);
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "InstantiateBytesResultResolver::promise_";
  static constexpr char kGlobalModuleHandle[] =
      "InstantiateBytesResultResolver::module_";
  Isolate* isolate_;
  Global<Context> context_;
  Global<Promise::Resolver> promise_resolver_;
  Global<Value> module_;
};

constexpr char InstantiateBytesResultResolver::kGlobalPromiseHandle[];
constexpr char InstantiateBytesResultResolver::kGlobalModuleHandle[];

// This class is the {CompilationResultResolver} for
// WebAssembly.instantiate(bytes, imports). When compilation finishes,
// {AsyncInstantiate} is started on the compilation result.
class AsyncInstantiateCompileResultResolver
    : public i::wasm::CompilationResultResolver {
 public:
  AsyncInstantiateCompileResultResolver(
      Isolate* isolate, Local<Context> context,
      Local<Promise::Resolver> promise_resolver, Local<Value> imports)
      : isolate_(isolate),
        context_(isolate, context),
        promise_resolver_(isolate, promise_resolver),
        imports_(isolate, imports) {
    context_.SetWeak();
    promise_resolver_.AnnotateStrongRetainer(kGlobalPromiseHandle);
    imports_.AnnotateStrongRetainer(kGlobalImportsHandle);
  }

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> result) override {
    if (finished_) return;
    finished_ = true;
    i::wasm::GetWasmEngine()->AsyncInstantiate(
        reinterpret_cast<i::Isolate*>(isolate_),
        std::make_unique<InstantiateBytesResultResolver>(
            isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
            Utils::ToLocal(i::Handle<i::Object>::cast(result))),
        result, ImportsAsMaybeReceiver(imports_.Get(isolate_)));
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    if (finished_) return;
    finished_ = true;
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<i::Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(error_reason), WasmAsyncSuccess::kFail);
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "AsyncInstantiateCompileResultResolver::promise_";
  static constexpr char kGlobalImportsHandle[] =
      "AsyncInstantiateCompileResultResolver::module_";
  bool finished_ = false;
  Isolate* isolate_;
  Global<Context> context_;
  Global<Promise::Resolver> promise_resolver_;
  Global<Value> imports_;
};

constexpr char AsyncInstantiateCompileResultResolver::kGlobalPromiseHandle[];
constexpr char AsyncInstantiateCompileResultResolver::kGlobalImportsHandle[];

std::string ToString(const char* name) { return std::string(name); }

std::string ToString(const i::Handle<i::String> name) {
  return std::string("Property '") + name->ToCString().get() + "'";
}

// Web IDL: '[EnforceRange] unsigned long'
// Previously called ToNonWrappingUint32 in the draft WebAssembly JS spec.
// https://heycam.github.io/webidl/#EnforceRange
template <typename T>
bool EnforceUint32(T argument_name, Local<v8::Value> v, Local<Context> context,
                   ErrorThrower* thrower, uint32_t* res) {
  double double_number;

  if (!v->NumberValue(context).To(&double_number)) {
    thrower->TypeError("%s must be convertible to a number",
                       ToString(argument_name).c_str());
    return false;
  }
  if (!std::isfinite(double_number)) {
    thrower->TypeError("%s must be convertible to a valid number",
                       ToString(argument_name).c_str());
    return false;
  }
  if (double_number < 0) {
    thrower->TypeError("%s must be non-negative",
                       ToString(argument_name).c_str());
    return false;
  }
  if (double_number > std::numeric_limits<uint32_t>::max()) {
    thrower->TypeError("%s must be in the unsigned long range",
                       ToString(argument_name).c_str());
    return false;
  }

  *res = static_cast<uint32_t>(double_number);
  return true;
}
}  // namespace

// WebAssembly.compile(bytes) -> Promise
void WebAssemblyCompile(const v8::FunctionCallbackInfo<v8::Value>& args) {
  constexpr const char* kAPIMethodName = "WebAssembly.compile()";
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, kAPIMethodName);

  i::Handle<i::Context> native_context = i_isolate->native_context();
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, native_context)) {
    i::Handle<i::String> error =
        i::wasm::ErrorStringForCodegen(i_isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
  }

  Local<Context> context = isolate->GetCurrentContext();
  ASSIGN(Promise::Resolver, promise_resolver, Promise::Resolver::New(context));
  Local<Promise> promise = promise_resolver->GetPromise();
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(promise);

  std::shared_ptr<i::wasm::CompilationResultResolver> resolver(
      new AsyncCompilationResolver(isolate, context, promise_resolver));

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);
  if (thrower.error()) {
    resolver->OnCompilationFailed(thrower.Reify());
    return;
  }
  // Asynchronous compilation handles copying wire bytes if necessary.
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
  i::wasm::GetWasmEngine()->AsyncCompile(i_isolate, enabled_features,
                                         std::move(resolver), bytes, is_shared,
                                         kAPIMethodName);
}

void WasmStreamingCallbackForTesting(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.compile()");

  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());

  bool is_shared = false;
  i::wasm::ModuleWireBytes bytes =
      GetFirstArgumentAsBytes(args, &thrower, &is_shared);
  if (thrower.error()) {
    streaming->Abort(Utils::ToLocal(thrower.Reify()));
    return;
  }
  streaming->OnBytesReceived(bytes.start(), bytes.length());
  streaming->Finish();
  CHECK(!thrower.error());
}

void WasmStreamingPromiseFailedCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->Abort(args[0]);
}

// WebAssembly.compileStreaming(Response | Promise<Response>)
//   -> Promise<WebAssembly.Module>
void WebAssemblyCompileStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  const char* const kAPIMethodName = "WebAssembly.compileStreaming()";
  ScheduledErrorThrower thrower(i_isolate, kAPIMethodName);
  Local<Context> context = isolate->GetCurrentContext();

  // Create and assign the return value of this function.
  ASSIGN(Promise::Resolver, promise_resolver, Promise::Resolver::New(context));
  Local<Promise> promise = promise_resolver->GetPromise();
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(promise);

  // Prepare the CompilationResultResolver for the compilation.
  auto resolver = std::make_shared<AsyncCompilationResolver>(isolate, context,
                                                             promise_resolver);

  i::Handle<i::Context> native_context = i_isolate->native_context();
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, native_context)) {
    i::Handle<i::String> error =
        i::wasm::ErrorStringForCodegen(i_isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
    resolver->OnCompilationFailed(thrower.Reify());
    return;
  }

  // Allocate the streaming decoder in a Managed so we can pass it to the
  // embedder.
  i::Handle<i::Managed<WasmStreaming>> data =
      i::Managed<WasmStreaming>::Allocate(
          i_isolate, 0,
          std::make_unique<WasmStreaming::WasmStreamingImpl>(
              isolate, kAPIMethodName, resolver));

  DCHECK_NOT_NULL(i_isolate->wasm_streaming_callback());
  ASSIGN(
      v8::Function, compile_callback,
      v8::Function::New(context, i_isolate->wasm_streaming_callback(),
                        Utils::ToLocal(i::Handle<i::Object>::cast(data)), 1));
  ASSIGN(
      v8::Function, reject_callback,
      v8::Function::New(context, WasmStreamingPromiseFailedCallback,
                        Utils::ToLocal(i::Handle<i::Object>::cast(data)), 1));

  // The parameter may be of type {Response} or of type {Promise<Response>}.
  // Treat either case of parameter as Promise.resolve(parameter)
  // as per https://www.w3.org/2001/tag/doc/promises-guide#resolve-arguments

  // Ending with:
  //    return Promise.resolve(parameter).then(compile_callback);
  ASSIGN(Promise::Resolver, input_resolver, Promise::Resolver::New(context));
  if (!input_resolver->Resolve(context, args[0]).IsJust()) return;

  // We do not have any use of the result here. The {compile_callback} will
  // start streaming compilation, which will eventually resolve the promise we
  // set as result value.
  USE(input_resolver->GetPromise()->Then(context, compile_callback,
                                         reject_callback));
}

// WebAssembly.validate(bytes) -> bool
void WebAssemblyValidate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.validate()");

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();

  if (thrower.error()) {
    if (thrower.wasm_error()) thrower.Reset();  // Clear error.
    return_value.Set(v8::False(isolate));
    return;
  }

  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
  bool validated = false;
  if (is_shared) {
    // Make a copy of the wire bytes to avoid concurrent modification.
    std::unique_ptr<uint8_t[]> copy(new uint8_t[bytes.length()]);
    memcpy(copy.get(), bytes.start(), bytes.length());
    i::wasm::ModuleWireBytes bytes_copy(copy.get(),
                                        copy.get() + bytes.length());
    validated = i::wasm::GetWasmEngine()->SyncValidate(
        i_isolate, enabled_features, bytes_copy);
  } else {
    // The wire bytes are not shared, OK to use them directly.
    validated = i::wasm::GetWasmEngine()->SyncValidate(i_isolate,
                                                       enabled_features, bytes);
  }

  return_value.Set(Boolean::New(isolate, validated));
}

namespace {
bool TransferPrototype(i::Isolate* isolate, i::Handle<i::JSObject> destination,
                       i::Handle<i::JSReceiver> source) {
  i::MaybeHandle<i::HeapObject> maybe_prototype =
      i::JSObject::GetPrototype(isolate, source);
  i::Handle<i::HeapObject> prototype;
  if (maybe_prototype.ToHandle(&prototype)) {
    Maybe<bool> result = i::JSObject::SetPrototype(
        isolate, destination, prototype,
        /*from_javascript=*/false, internal::kThrowOnError);
    if (!result.FromJust()) {
      DCHECK(isolate->has_pending_exception());
      return false;
    }
  }
  return true;
}
}  // namespace

// new WebAssembly.Module(bytes) -> WebAssembly.Module
void WebAssemblyModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (i_isolate->wasm_module_callback()(args)) return;

  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module()");

  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Module must be invoked with 'new'");
    return;
  }
  i::Handle<i::Context> native_context = i_isolate->native_context();
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, native_context)) {
    i::Handle<i::String> error =
        i::wasm::ErrorStringForCodegen(i_isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
    return;
  }

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);

  if (thrower.error()) {
    return;
  }
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
  i::MaybeHandle<i::WasmModuleObject> maybe_module_obj;
  if (is_shared) {
    // Make a copy of the wire bytes to avoid concurrent modification.
    std::unique_ptr<uint8_t[]> copy(new uint8_t[bytes.length()]);
    memcpy(copy.get(), bytes.start(), bytes.length());
    i::wasm::ModuleWireBytes bytes_copy(copy.get(),
                                        copy.get() + bytes.length());
    maybe_module_obj = i::wasm::GetWasmEngine()->SyncCompile(
        i_isolate, enabled_features, &thrower, bytes_copy);
  } else {
    // The wire bytes are not shared, OK to use them directly.
    maybe_module_obj = i::wasm::GetWasmEngine()->SyncCompile(
        i_isolate, enabled_features, &thrower, bytes);
  }

  i::Handle<i::WasmModuleObject> module_obj;
  if (!maybe_module_obj.ToHandle(&module_obj)) return;

  // The infrastructure for `new Foo` calls allocates an object, which is
  // available here as {args.This()}. We're going to discard this object
  // and use {module_obj} instead, but it does have the correct prototype,
  // which we must harvest from it. This makes a difference when the JS
  // constructor function wasn't {WebAssembly.Module} directly, but some
  // subclass: {module_obj} has {WebAssembly.Module}'s prototype at this
  // point, so we must overwrite that with the correct prototype for {Foo}.
  if (!TransferPrototype(i_isolate, module_obj,
                         Utils::OpenHandle(*args.This()))) {
    return;
  }

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(i::Handle<i::JSObject>::cast(module_obj)));
}

// WebAssembly.Module.imports(module) -> Array<Import>
void WebAssemblyModuleImports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module.imports()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;
  auto imports = i::wasm::GetImports(i_isolate, maybe_module.ToHandleChecked());
  args.GetReturnValue().Set(Utils::ToLocal(imports));
}

// WebAssembly.Module.exports(module) -> Array<Export>
void WebAssemblyModuleExports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module.exports()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;
  auto exports = i::wasm::GetExports(i_isolate, maybe_module.ToHandleChecked());
  args.GetReturnValue().Set(Utils::ToLocal(exports));
}

// WebAssembly.Module.customSections(module, name) -> Array<Section>
void WebAssemblyModuleCustomSections(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate,
                                "WebAssembly.Module.customSections()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;

  if (args[1]->IsUndefined()) {
    thrower.TypeError("Argument 1 is required");
    return;
  }

  i::MaybeHandle<i::Object> maybe_name =
      i::Object::ToString(i_isolate, Utils::OpenHandle(*args[1]));
  i::Handle<i::Object> name;
  if (!maybe_name.ToHandle(&name)) return;
  auto custom_sections =
      i::wasm::GetCustomSections(i_isolate, maybe_module.ToHandleChecked(),
                                 i::Handle<i::String>::cast(name), &thrower);
  if (thrower.error()) return;
  args.GetReturnValue().Set(Utils::ToLocal(custom_sections));
}

// new WebAssembly.Instance(module, imports) -> WebAssembly.Instance
void WebAssemblyInstance(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  HandleScope scope(args.GetIsolate());
  if (i_isolate->wasm_instance_callback()(args)) return;

  i::MaybeHandle<i::JSObject> maybe_instance_obj;
  {
    ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Instance()");
    if (!args.IsConstructCall()) {
      thrower.TypeError("WebAssembly.Instance must be invoked with 'new'");
      return;
    }

    i::MaybeHandle<i::WasmModuleObject> maybe_module =
        GetFirstArgumentAsModule(args, &thrower);
    if (thrower.error()) return;

    i::Handle<i::WasmModuleObject> module_obj = maybe_module.ToHandleChecked();

    Local<Value> ffi = args[1];

    if (!ffi->IsUndefined() && !ffi->IsObject()) {
      thrower.TypeError("Argument 1 must be an object");
      return;
    }
    if (thrower.error()) return;

    maybe_instance_obj = i::wasm::GetWasmEngine()->SyncInstantiate(
        i_isolate, &thrower, module_obj, ImportsAsMaybeReceiver(ffi),
        i::MaybeHandle<i::JSArrayBuffer>());
  }

  i::Handle<i::JSObject> instance_obj;
  if (!maybe_instance_obj.ToHandle(&instance_obj)) {
    DCHECK(i_isolate->has_scheduled_exception());
    return;
  }

  // The infrastructure for `new Foo` calls allocates an object, which is
  // available here as {args.This()}. We're going to discard this object
  // and use {instance_obj} instead, but it does have the correct prototype,
  // which we must harvest from it. This makes a difference when the JS
  // constructor function wasn't {WebAssembly.Instance} directly, but some
  // subclass: {instance_obj} has {WebAssembly.Instance}'s prototype at this
  // point, so we must overwrite that with the correct prototype for {Foo}.
  if (!TransferPrototype(i_isolate, instance_obj,
                         Utils::OpenHandle(*args.This()))) {
    return;
  }

  args.GetReturnValue().Set(Utils::ToLocal(instance_obj));
}

// WebAssembly.instantiateStreaming(Response | Promise<Response> [, imports])
//   -> Promise<ResultObject>
// (where ResultObject has a "module" and an "instance" field)
void WebAssemblyInstantiateStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  const char* const kAPIMethodName = "WebAssembly.instantiateStreaming()";
  ScheduledErrorThrower thrower(i_isolate, kAPIMethodName);

  // Create and assign the return value of this function.
  ASSIGN(Promise::Resolver, result_resolver, Promise::Resolver::New(context));
  Local<Promise> promise = result_resolver->GetPromise();
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(promise);

  // Create an InstantiateResultResolver in case there is an issue with the
  // passed parameters.
  std::unique_ptr<i::wasm::InstantiationResultResolver> resolver(
      new InstantiateModuleResultResolver(isolate, context, result_resolver));

  i::Handle<i::Context> native_context = i_isolate->native_context();
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, native_context)) {
    i::Handle<i::String> error =
        i::wasm::ErrorStringForCodegen(i_isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
  Local<Value> ffi = args[1];

  if (!ffi->IsUndefined() && !ffi->IsObject()) {
    thrower.TypeError("Argument 1 must be an object");
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  // We start compilation now, we have no use for the
  // {InstantiationResultResolver}.
  resolver.reset();

  std::shared_ptr<i::wasm::CompilationResultResolver> compilation_resolver(
      new AsyncInstantiateCompileResultResolver(isolate, context,
                                                result_resolver, ffi));

  // Allocate the streaming decoder in a Managed so we can pass it to the
  // embedder.
  i::Handle<i::Managed<WasmStreaming>> data =
      i::Managed<WasmStreaming>::Allocate(
          i_isolate, 0,
          std::make_unique<WasmStreaming::WasmStreamingImpl>(
              isolate, kAPIMethodName, compilation_resolver));

  DCHECK_NOT_NULL(i_isolate->wasm_streaming_callback());
  ASSIGN(
      v8::Function, compile_callback,
      v8::Function::New(context, i_isolate->wasm_streaming_callback(),
                        Utils::ToLocal(i::Handle<i::Object>::cast(data)), 1));
  ASSIGN(
      v8::Function, reject_callback,
      v8::Function::New(context, WasmStreamingPromiseFailedCallback,
                        Utils::ToLocal(i::Handle<i::Object>::cast(data)), 1));

  // The parameter may be of type {Response} or of type {Promise<Response>}.
  // Treat either case of parameter as Promise.resolve(parameter)
  // as per https://www.w3.org/2001/tag/doc/promises-guide#resolve-arguments

  // Ending with:
  //    return Promise.resolve(parameter).then(compile_callback);
  ASSIGN(Promise::Resolver, input_resolver, Promise::Resolver::New(context));
  if (!input_resolver->Resolve(context, args[0]).IsJust()) return;

  // We do not have any use of the result here. The {compile_callback} will
  // start streaming compilation, which will eventually resolve the promise we
  // set as result value.
  USE(input_resolver->GetPromise()->Then(context, compile_callback,
                                         reject_callback));
}

// WebAssembly.instantiate(module, imports) -> WebAssembly.Instance
// WebAssembly.instantiate(bytes, imports) ->
//     {module: WebAssembly.Module, instance: WebAssembly.Instance}
void WebAssemblyInstantiate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  constexpr const char* kAPIMethodName = "WebAssembly.instantiate()";
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  ScheduledErrorThrower thrower(i_isolate, kAPIMethodName);

  HandleScope scope(isolate);

  Local<Context> context = isolate->GetCurrentContext();

  ASSIGN(Promise::Resolver, promise_resolver, Promise::Resolver::New(context));
  Local<Promise> promise = promise_resolver->GetPromise();
  args.GetReturnValue().Set(promise);

  std::unique_ptr<i::wasm::InstantiationResultResolver> resolver(
      new InstantiateModuleResultResolver(isolate, context, promise_resolver));

  Local<Value> first_arg_value = args[0];
  i::Handle<i::Object> first_arg = Utils::OpenHandle(*first_arg_value);
  if (!first_arg->IsJSObject()) {
    thrower.TypeError(
        "Argument 0 must be a buffer source or a WebAssembly.Module object");
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
  Local<Value> ffi = args[1];

  if (!ffi->IsUndefined() && !ffi->IsObject()) {
    thrower.TypeError("Argument 1 must be an object");
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  if (first_arg->IsWasmModuleObject()) {
    i::Handle<i::WasmModuleObject> module_obj =
        i::Handle<i::WasmModuleObject>::cast(first_arg);

    i::wasm::GetWasmEngine()->AsyncInstantiate(i_isolate, std::move(resolver),
                                               module_obj,
                                               ImportsAsMaybeReceiver(ffi));
    return;
  }

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);
  if (thrower.error()) {
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  // We start compilation now, we have no use for the
  // {InstantiationResultResolver}.
  resolver.reset();

  std::shared_ptr<i::wasm::CompilationResultResolver> compilation_resolver(
      new AsyncInstantiateCompileResultResolver(isolate, context,
                                                promise_resolver, ffi));

  // The first parameter is a buffer source, we have to check if we are allowed
  // to compile it.
  i::Handle<i::Context> native_context = i_isolate->native_context();
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, native_context)) {
    i::Handle<i::String> error =
        i::wasm::ErrorStringForCodegen(i_isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
    compilation_resolver->OnCompilationFailed(thrower.Reify());
    return;
  }

  // Asynchronous compilation handles copying wire bytes if necessary.
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
  i::wasm::GetWasmEngine()->AsyncCompile(i_isolate, enabled_features,
                                         std::move(compilation_resolver), bytes,
                                         is_shared, kAPIMethodName);
}

bool GetIntegerProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                        Local<Context> context, v8::Local<v8::Value> value,
                        i::Handle<i::String> property_name, int64_t* result,
                        int64_t lower_bound, uint64_t upper_bound) {
  uint32_t number;
  if (!EnforceUint32(property_name, value, context, thrower, &number)) {
    return false;
  }
  if (number < lower_bound) {
    thrower->RangeError("Property '%s': value %" PRIu32
                        " is below the lower bound %" PRIx64,
                        property_name->ToCString().get(), number, lower_bound);
    return false;
  }
  if (number > upper_bound) {
    thrower->RangeError("Property '%s': value %" PRIu32
                        " is above the upper bound %" PRIu64,
                        property_name->ToCString().get(), number, upper_bound);
    return false;
  }

  *result = static_cast<int64_t>(number);
  return true;
}

bool GetOptionalIntegerProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                                Local<Context> context,
                                Local<v8::Object> object,
                                Local<String> property, bool* has_property,
                                int64_t* result, int64_t lower_bound,
                                uint64_t upper_bound) {
  v8::Local<v8::Value> value;
  if (!object->Get(context, property).ToLocal(&value)) {
    return false;
  }

  // Web IDL: dictionary presence
  // https://heycam.github.io/webidl/#dfn-present
  if (value->IsUndefined()) {
    if (has_property != nullptr) *has_property = false;
    return true;
  }

  if (has_property != nullptr) *has_property = true;
  i::Handle<i::String> property_name = v8::Utils::OpenHandle(*property);

  return GetIntegerProperty(isolate, thrower, context, value, property_name,
                            result, lower_bound, upper_bound);
}

// Fetch 'initial' or 'minimum' property from object. If both are provided,
// a TypeError is thrown.
// TODO(aseemgarg): change behavior when the following bug is resolved:
// https://github.com/WebAssembly/js-types/issues/6
bool GetInitialOrMinimumProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                                 Local<Context> context,
                                 Local<v8::Object> object, int64_t* result,
                                 int64_t lower_bound, uint64_t upper_bound) {
  bool has_initial = false;
  if (!GetOptionalIntegerProperty(isolate, thrower, context, object,
                                  v8_str(isolate, "initial"), &has_initial,
                                  result, lower_bound, upper_bound)) {
    return false;
  }
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(
      reinterpret_cast<i::Isolate*>(isolate));
  if (enabled_features.has_type_reflection()) {
    bool has_minimum = false;
    int64_t minimum = 0;
    if (!GetOptionalIntegerProperty(isolate, thrower, context, object,
                                    v8_str(isolate, "minimum"), &has_minimum,
                                    &minimum, lower_bound, upper_bound)) {
      return false;
    }
    if (has_initial && has_minimum) {
      thrower->TypeError(
          "The properties 'initial' and 'minimum' are not allowed at the same "
          "time");
      return false;
    }
    if (has_minimum) {
      // Only {minimum} exists, so we use {minimum} as {initial}.
      has_initial = true;
      *result = minimum;
    }
  }
  if (!has_initial) {
    // TODO(aseemgarg): update error message when the spec issue is resolved.
    thrower->TypeError("Property 'initial' is required");
    return false;
  }
  return true;
}

namespace {
i::Handle<i::Object> DefaultReferenceValue(i::Isolate* isolate,
                                           i::wasm::ValueType type) {
  if (type.is_reference()) {
    // Use undefined for JS type (externref) but null for wasm types as wasm
    // does not know undefined.
    if (type.heap_representation() == i::wasm::HeapType::kExtern) {
      return isolate->factory()->undefined_value();
    }
    return isolate->factory()->null_value();
  }
  UNREACHABLE();
}
}  // namespace

// new WebAssembly.Table(args) -> WebAssembly.Table
void WebAssemblyTable(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Table must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a table descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);
  i::wasm::ValueType type;
  // The descriptor's 'element'.
  {
    v8::MaybeLocal<v8::Value> maybe =
        descriptor->Get(context, v8_str(isolate, "element"));
    v8::Local<v8::Value> value;
    if (!maybe.ToLocal(&value)) return;
    v8::Local<v8::String> string;
    if (!value->ToString(context).ToLocal(&string)) return;
    auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
    // The JS api uses 'anyfunc' instead of 'funcref'.
    if (string->StringEquals(v8_str(isolate, "anyfunc"))) {
      type = i::wasm::kWasmFuncRef;
    } else if (enabled_features.has_type_reflection() &&
               string->StringEquals(v8_str(isolate, "funcref"))) {
      // With the type reflection proposal, "funcref" replaces "anyfunc",
      // and anyfunc just becomes an alias for "funcref".
      type = i::wasm::kWasmFuncRef;
    } else if (string->StringEquals(v8_str(isolate, "externref"))) {
      type = i::wasm::kWasmExternRef;
    } else if (enabled_features.has_stringref() &&
               string->StringEquals(v8_str(isolate, "stringref"))) {
      type = i::wasm::kWasmStringRef;
    } else if (enabled_features.has_gc() &&
               string->StringEquals(v8_str(isolate, "anyref"))) {
      type = i::wasm::kWasmAnyRef;
    } else if (enabled_features.has_gc() &&
               string->StringEquals(v8_str(isolate, "eqref"))) {
      type = i::wasm::kWasmEqRef;
    } else if (enabled_features.has_gc() &&
               string->StringEquals(v8_str(isolate, "dataref"))) {
      type = i::wasm::kWasmDataRef;
    } else if (enabled_features.has_gc() &&
               string->StringEquals(v8_str(isolate, "arrayref"))) {
      type = i::wasm::kWasmArrayRef;
    } else {
      // TODO(7748): Add "i31ref".
      thrower.TypeError(
          "Descriptor property 'element' must be a WebAssembly reference type");
      return;
    }
  }

  int64_t initial = 0;
  if (!GetInitialOrMinimumProperty(isolate, &thrower, context, descriptor,
                                   &initial, 0,
                                   i::wasm::max_table_init_entries())) {
    return;
  }
  // The descriptor's 'maximum'.
  int64_t maximum = -1;
  bool has_maximum = true;
  if (!GetOptionalIntegerProperty(isolate, &thrower, context, descriptor,
                                  v8_str(isolate, "maximum"), &has_maximum,
                                  &maximum, initial,
                                  std::numeric_limits<uint32_t>::max())) {
    return;
  }

  i::Handle<i::FixedArray> fixed_array;
  i::Handle<i::WasmTableObject> table_obj =
      i::WasmTableObject::New(i_isolate, i::Handle<i::WasmInstanceObject>(),
                              type, static_cast<uint32_t>(initial), has_maximum,
                              static_cast<uint32_t>(maximum), &fixed_array,
                              DefaultReferenceValue(i_isolate, type));

  // The infrastructure for `new Foo` calls allocates an object, which is
  // available here as {args.This()}. We're going to discard this object
  // and use {table_obj} instead, but it does have the correct prototype,
  // which we must harvest from it. This makes a difference when the JS
  // constructor function wasn't {WebAssembly.Table} directly, but some
  // subclass: {table_obj} has {WebAssembly.Table}'s prototype at this
  // point, so we must overwrite that with the correct prototype for {Foo}.
  if (!TransferPrototype(i_isolate, table_obj,
                         Utils::OpenHandle(*args.This()))) {
    return;
  }

  if (initial > 0 && args.Length() >= 2 && !args[1]->IsUndefined()) {
    i::Handle<i::Object> element = Utils::OpenHandle(*args[1]);
    const char* error_message;
    if (!i::WasmTableObject::JSToWasmElement(i_isolate, table_obj, element,
                                             &error_message)
             .ToHandle(&element)) {
      thrower.TypeError(
          "Argument 2 must be undefined or a value of type compatible "
          "with the type of the new table: %s.",
          error_message);
      return;
    }
    for (uint32_t index = 0; index < static_cast<uint32_t>(initial); ++index) {
      i::WasmTableObject::Set(i_isolate, table_obj, index, element);
    }
  } else if (initial > 0) {
    switch (table_obj->type().heap_representation()) {
      case i::wasm::HeapType::kString:
        thrower.TypeError(
            "Missing initial value when creating stringref table");
        return;
      case i::wasm::HeapType::kStringViewWtf8:
        thrower.TypeError("stringview_wtf8 has no JS representation");
        return;
      case i::wasm::HeapType::kStringViewWtf16:
        thrower.TypeError("stringview_wtf16 has no JS representation");
        return;
      case i::wasm::HeapType::kStringViewIter:
        thrower.TypeError("stringview_iter has no JS representation");
        return;
      default:
        break;
    }
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(i::Handle<i::JSObject>::cast(table_obj)));
}

void WebAssemblyMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Memory must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a memory descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);

  // TODO(clemensb): The JS API spec is not updated for memory64 yet; fix this
  // code once it is.
  int64_t initial = 0;
  if (!GetInitialOrMinimumProperty(isolate, &thrower, context, descriptor,
                                   &initial, 0,
                                   i::wasm::kSpecMaxMemory32Pages)) {
    return;
  }
  // The descriptor's 'maximum'.
  int64_t maximum = i::WasmMemoryObject::kNoMaximum;
  if (!GetOptionalIntegerProperty(isolate, &thrower, context, descriptor,
                                  v8_str(isolate, "maximum"), nullptr, &maximum,
                                  initial, i::wasm::kSpecMaxMemory32Pages)) {
    return;
  }

  auto shared = i::SharedFlag::kNotShared;
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
  if (enabled_features.has_threads()) {
    // Shared property of descriptor
    Local<String> shared_key = v8_str(isolate, "shared");
    v8::MaybeLocal<v8::Value> maybe_value =
        descriptor->Get(context, shared_key);
    v8::Local<v8::Value> value;
    if (maybe_value.ToLocal(&value)) {
      shared = value->BooleanValue(isolate) ? i::SharedFlag::kShared
                                            : i::SharedFlag::kNotShared;
    } else {
      DCHECK(i_isolate->has_scheduled_exception());
      return;
    }

    // Throw TypeError if shared is true, and the descriptor has no "maximum"
    if (shared == i::SharedFlag::kShared && maximum == -1) {
      thrower.TypeError(
          "If shared is true, maximum property should be defined.");
      return;
    }
  }

  i::Handle<i::JSObject> memory_obj;
  if (!i::WasmMemoryObject::New(i_isolate, static_cast<int>(initial),
                                static_cast<int>(maximum), shared)
           .ToHandle(&memory_obj)) {
    thrower.RangeError("could not allocate memory");
    return;
  }

  // The infrastructure for `new Foo` calls allocates an object, which is
  // available here as {args.This()}. We're going to discard this object
  // and use {memory_obj} instead, but it does have the correct prototype,
  // which we must harvest from it. This makes a difference when the JS
  // constructor function wasn't {WebAssembly.Memory} directly, but some
  // subclass: {memory_obj} has {WebAssembly.Memory}'s prototype at this
  // point, so we must overwrite that with the correct prototype for {Foo}.
  if (!TransferPrototype(i_isolate, memory_obj,
                         Utils::OpenHandle(*args.This()))) {
    return;
  }

  if (shared == i::SharedFlag::kShared) {
    i::Handle<i::JSArrayBuffer> buffer(
        i::Handle<i::WasmMemoryObject>::cast(memory_obj)->array_buffer(),
        i_isolate);
    Maybe<bool> result =
        buffer->SetIntegrityLevel(buffer, i::FROZEN, i::kDontThrow);
    if (!result.FromJust()) {
      thrower.TypeError(
          "Status of setting SetIntegrityLevel of buffer is false.");
      return;
    }
  }
  args.GetReturnValue().Set(Utils::ToLocal(memory_obj));
}

// Determines the type encoded in a value type property (e.g. type reflection).
// Returns false if there was an exception, true upon success. On success the
// outgoing {type} is set accordingly, or set to {wasm::kWasmVoid} in case the
// type could not be properly recognized.
bool GetValueType(Isolate* isolate, MaybeLocal<Value> maybe,
                  Local<Context> context, i::wasm::ValueType* type,
                  i::wasm::WasmFeatures enabled_features) {
  v8::Local<v8::Value> value;
  if (!maybe.ToLocal(&value)) return false;
  v8::Local<v8::String> string;
  if (!value->ToString(context).ToLocal(&string)) return false;
  if (string->StringEquals(v8_str(isolate, "i32"))) {
    *type = i::wasm::kWasmI32;
  } else if (string->StringEquals(v8_str(isolate, "f32"))) {
    *type = i::wasm::kWasmF32;
  } else if (string->StringEquals(v8_str(isolate, "i64"))) {
    *type = i::wasm::kWasmI64;
  } else if (string->StringEquals(v8_str(isolate, "f64"))) {
    *type = i::wasm::kWasmF64;
  } else if (string->StringEquals(v8_str(isolate, "externref"))) {
    *type = i::wasm::kWasmExternRef;
  } else if (enabled_features.has_type_reflection() &&
             string->StringEquals(v8_str(isolate, "funcref"))) {
    // The type reflection proposal renames "anyfunc" to "funcref", and makes
    // "anyfunc" an alias of "funcref".
    *type = i::wasm::kWasmFuncRef;
  } else if (string->StringEquals(v8_str(isolate, "anyfunc"))) {
    // The JS api spec uses 'anyfunc' instead of 'funcref'.
    *type = i::wasm::kWasmFuncRef;
  } else if (enabled_features.has_gc() &&
             string->StringEquals(v8_str(isolate, "eqref"))) {
    *type = i::wasm::kWasmEqRef;
  } else if (enabled_features.has_stringref() &&
             string->StringEquals(v8_str(isolate, "stringref"))) {
    *type = i::wasm::kWasmStringRef;
  } else if (enabled_features.has_gc() &&
             string->StringEquals(v8_str(isolate, "anyref"))) {
    *type = i::wasm::kWasmAnyRef;
  } else if (enabled_features.has_gc() &&
             string->StringEquals(v8_str(isolate, "dataref"))) {
    *type = i::wasm::kWasmDataRef;
  } else if (enabled_features.has_gc() &&
             string->StringEquals(v8_str(isolate, "arrayref"))) {
    *type = i::wasm::kWasmArrayRef;
  } else {
    // Unrecognized type.
    // TODO(7748): Add "i31ref".
    *type = i::wasm::kWasmVoid;
  }
  return true;
}

namespace {

bool ToI32(Local<v8::Value> value, Local<Context> context, int32_t* i32_value) {
  if (!value->IsUndefined()) {
    v8::Local<v8::Int32> int32_value;
    if (!value->ToInt32(context).ToLocal(&int32_value)) return false;
    if (!int32_value->Int32Value(context).To(i32_value)) return false;
  }
  return true;
}

bool ToI64(Local<v8::Value> value, Local<Context> context, int64_t* i64_value) {
  if (!value->IsUndefined()) {
    v8::Local<v8::BigInt> bigint_value;
    if (!value->ToBigInt(context).ToLocal(&bigint_value)) return false;
    *i64_value = bigint_value->Int64Value();
  }
  return true;
}

bool ToF32(Local<v8::Value> value, Local<Context> context, float* f32_value) {
  if (!value->IsUndefined()) {
    double f64_value = 0;
    v8::Local<v8::Number> number_value;
    if (!value->ToNumber(context).ToLocal(&number_value)) return false;
    if (!number_value->NumberValue(context).To(&f64_value)) return false;
    *f32_value = i::DoubleToFloat32(f64_value);
  }
  return true;
}

bool ToF64(Local<v8::Value> value, Local<Context> context, double* f64_value) {
  if (!value->IsUndefined()) {
    v8::Local<v8::Number> number_value;
    if (!value->ToNumber(context).ToLocal(&number_value)) return false;
    if (!number_value->NumberValue(context).To(f64_value)) return false;
  }
  return true;
}
}  // namespace

// WebAssembly.Global
void WebAssemblyGlobal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Global()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Global must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a global descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);

  // The descriptor's 'mutable'.
  bool is_mutable = false;
  {
    Local<String> mutable_key = v8_str(isolate, "mutable");
    v8::MaybeLocal<v8::Value> maybe = descriptor->Get(context, mutable_key);
    v8::Local<v8::Value> value;
    if (maybe.ToLocal(&value)) {
      is_mutable = value->BooleanValue(isolate);
    } else {
      DCHECK(i_isolate->has_scheduled_exception());
      return;
    }
  }

  // The descriptor's type, called 'value'. It is called 'value' because this
  // descriptor is planned to be re-used as the global's type for reflection,
  // so calling it 'type' is redundant.
  i::wasm::ValueType type;
  {
    v8::MaybeLocal<v8::Value> maybe =
        descriptor->Get(context, v8_str(isolate, "value"));
    if (!GetValueType(isolate, maybe, context, &type, enabled_features)) return;
    if (type == i::wasm::kWasmVoid) {
      thrower.TypeError(
          "Descriptor property 'value' must be a WebAssembly type");
      return;
    }
  }

  const uint32_t offset = 0;
  i::MaybeHandle<i::WasmGlobalObject> maybe_global_obj =
      i::WasmGlobalObject::New(i_isolate, i::Handle<i::WasmInstanceObject>(),
                               i::MaybeHandle<i::JSArrayBuffer>(),
                               i::MaybeHandle<i::FixedArray>(), type, offset,
                               is_mutable);

  i::Handle<i::WasmGlobalObject> global_obj;
  if (!maybe_global_obj.ToHandle(&global_obj)) {
    thrower.RangeError("could not allocate memory");
    return;
  }

  // The infrastructure for `new Foo` calls allocates an object, which is
  // available here as {args.This()}. We're going to discard this object
  // and use {global_obj} instead, but it does have the correct prototype,
  // which we must harvest from it. This makes a difference when the JS
  // constructor function wasn't {WebAssembly.Global} directly, but some
  // subclass: {global_obj} has {WebAssembly.Global}'s prototype at this
  // point, so we must overwrite that with the correct prototype for {Foo}.
  if (!TransferPrototype(i_isolate, global_obj,
                         Utils::OpenHandle(*args.This()))) {
    return;
  }

  // Convert value to a WebAssembly value, the default value is 0.
  Local<v8::Value> value = Local<Value>::Cast(args[1]);
  switch (type.kind()) {
    case i::wasm::kI32: {
      int32_t i32_value = 0;
      if (!ToI32(value, context, &i32_value)) return;
      global_obj->SetI32(i32_value);
      break;
    }
    case i::wasm::kI64: {
      int64_t i64_value = 0;
      if (!ToI64(value, context, &i64_value)) return;
      global_obj->SetI64(i64_value);
      break;
    }
    case i::wasm::kF32: {
      float f32_value = 0;
      if (!ToF32(value, context, &f32_value)) return;
      global_obj->SetF32(f32_value);
      break;
    }
    case i::wasm::kF64: {
      double f64_value = 0;
      if (!ToF64(value, context, &f64_value)) return;
      global_obj->SetF64(f64_value);
      break;
    }
    case i::wasm::kRef:
      if (args.Length() < 2) {
        thrower.TypeError("Non-defaultable global needs initial value");
        break;
      }
      V8_FALLTHROUGH;
    case i::wasm::kRefNull: {
      // We need the wasm default value {null} over {undefined}.
      i::Handle<i::Object> value_handle =
          (args.Length() < 2) ? i_isolate->factory()->null_value()
                              : Utils::OpenHandle(*value);
      const char* error_message;
      if (!i::wasm::JSToWasmObject(i_isolate, nullptr, value_handle, type,
                                   &error_message)
               .ToHandle(&value_handle)) {
        thrower.TypeError("%s", error_message);
        break;
      }
      global_obj->SetRef(value_handle);
      break;
    }
    case i::wasm::kRtt:
      // TODO(7748): Implement.
      UNIMPLEMENTED();
    case i::wasm::kI8:
    case i::wasm::kI16:
    case i::wasm::kVoid:
    case i::wasm::kS128:
    case i::wasm::kBottom:
      UNREACHABLE();
  }

  i::Handle<i::JSObject> global_js_object(global_obj);
  args.GetReturnValue().Set(Utils::ToLocal(global_js_object));
}

namespace {

uint32_t GetIterableLength(i::Isolate* isolate, Local<Context> context,
                           Local<Object> iterable) {
  Local<String> length = Utils::ToLocal(isolate->factory()->length_string());
  MaybeLocal<Value> property = iterable->Get(context, length);
  if (property.IsEmpty()) return i::kMaxUInt32;
  MaybeLocal<Uint32> number = property.ToLocalChecked()->ToArrayIndex(context);
  if (number.IsEmpty()) return i::kMaxUInt32;
  DCHECK_NE(i::kMaxUInt32, number.ToLocalChecked()->Value());
  return number.ToLocalChecked()->Value();
}

}  // namespace

// WebAssembly.Tag
void WebAssemblyTag(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);

  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Tag()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Tag must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a tag type");
    return;
  }

  Local<Object> event_type = Local<Object>::Cast(args[0]);
  Local<Context> context = isolate->GetCurrentContext();
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);

  // Load the 'parameters' property of the event type.
  Local<String> parameters_key = v8_str(isolate, "parameters");
  v8::MaybeLocal<v8::Value> parameters_maybe =
      event_type->Get(context, parameters_key);
  v8::Local<v8::Value> parameters_value;
  if (!parameters_maybe.ToLocal(&parameters_value) ||
      !parameters_value->IsObject()) {
    thrower.TypeError("Argument 0 must be a tag type with 'parameters'");
    return;
  }
  Local<Object> parameters = parameters_value.As<Object>();
  uint32_t parameters_len = GetIterableLength(i_isolate, context, parameters);
  if (parameters_len == i::kMaxUInt32) {
    thrower.TypeError("Argument 0 contains parameters without 'length'");
    return;
  }
  if (parameters_len > i::wasm::kV8MaxWasmFunctionParams) {
    thrower.TypeError("Argument 0 contains too many parameters");
    return;
  }

  // Decode the tag type and construct a signature.
  std::vector<i::wasm::ValueType> param_types(parameters_len,
                                              i::wasm::kWasmVoid);
  for (uint32_t i = 0; i < parameters_len; ++i) {
    i::wasm::ValueType& type = param_types[i];
    MaybeLocal<Value> maybe = parameters->Get(context, i);
    if (!GetValueType(isolate, maybe, context, &type, enabled_features) ||
        type == i::wasm::kWasmVoid) {
      thrower.TypeError(
          "Argument 0 parameter type at index #%u must be a value type", i);
      return;
    }
  }
  const i::wasm::FunctionSig sig{0, parameters_len, param_types.data()};
  // Set the tag index to 0. It is only used for debugging purposes, and has no
  // meaningful value when declared outside of a wasm module.
  auto tag = i::WasmExceptionTag::New(i_isolate, 0);
  i::Handle<i::JSObject> tag_object =
      i::WasmTagObject::New(i_isolate, &sig, tag);
  args.GetReturnValue().Set(Utils::ToLocal(tag_object));
}

// WebAssembly.Suspender
void WebAssemblySuspender(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);

  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Suspender()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Suspender must be invoked with 'new'");
    return;
  }

  i::Handle<i::JSObject> suspender = i::WasmSuspenderObject::New(i_isolate);

  // The infrastructure for `new Foo` calls allocates an object, which is
  // available here as {args.This()}. We're going to discard this object
  // and use {suspender} instead, but it does have the correct prototype,
  // which we must harvest from it. This makes a difference when the JS
  // constructor function wasn't {WebAssembly.Suspender} directly, but some
  // subclass: {suspender} has {WebAssembly.Suspender}'s prototype at this
  // point, so we must overwrite that with the correct prototype for {Foo}.
  if (!TransferPrototype(i_isolate, suspender,
                         Utils::OpenHandle(*args.This()))) {
    return;
  }
  args.GetReturnValue().Set(Utils::ToLocal(suspender));
}

namespace {

uint32_t GetEncodedSize(i::Handle<i::WasmTagObject> tag_object) {
  auto serialized_sig = tag_object->serialized_signature();
  i::wasm::WasmTagSig sig{0, static_cast<size_t>(serialized_sig.length()),
                          reinterpret_cast<i::wasm::ValueType*>(
                              serialized_sig.GetDataStartAddress())};
  i::wasm::WasmTag tag(&sig);
  return i::WasmExceptionPackage::GetEncodedSize(&tag);
}

void EncodeExceptionValues(v8::Isolate* isolate,
                           i::Handle<i::PodArray<i::wasm::ValueType>> signature,
                           const Local<Value>& arg,
                           ScheduledErrorThrower* thrower,
                           i::Handle<i::FixedArray> values_out) {
  Local<Context> context = isolate->GetCurrentContext();
  uint32_t index = 0;
  if (!arg->IsObject()) {
    thrower->TypeError("Exception values must be an iterable object");
    return;
  }
  auto values = arg.As<Object>();
  for (int i = 0; i < signature->length(); ++i) {
    MaybeLocal<Value> maybe_value = values->Get(context, i);
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    if (i_isolate->has_scheduled_exception()) return;
    Local<Value> value = maybe_value.ToLocalChecked();
    i::wasm::ValueType type = signature->get(i);
    switch (type.kind()) {
      case i::wasm::kI32: {
        int32_t i32 = 0;
        if (!ToI32(value, context, &i32)) return;
        i::EncodeI32ExceptionValue(values_out, &index, i32);
        break;
      }
      case i::wasm::kI64: {
        int64_t i64 = 0;
        if (!ToI64(value, context, &i64)) return;
        i::EncodeI64ExceptionValue(values_out, &index, i64);
        break;
      }
      case i::wasm::kF32: {
        float f32 = 0;
        if (!ToF32(value, context, &f32)) return;
        int32_t i32 = base::bit_cast<int32_t>(f32);
        i::EncodeI32ExceptionValue(values_out, &index, i32);
        break;
      }
      case i::wasm::kF64: {
        double f64 = 0;
        if (!ToF64(value, context, &f64)) return;
        int64_t i64 = base::bit_cast<int64_t>(f64);
        i::EncodeI64ExceptionValue(values_out, &index, i64);
        break;
      }
      case i::wasm::kRef:
      case i::wasm::kRefNull:
        switch (type.heap_representation()) {
          case i::wasm::HeapType::kFunc:
          case i::wasm::HeapType::kExtern:
          case i::wasm::HeapType::kAny:
          case i::wasm::HeapType::kEq:
          case i::wasm::HeapType::kI31:
          case i::wasm::HeapType::kData:
          case i::wasm::HeapType::kArray:
          case i::wasm::HeapType::kString:
          case i::wasm::HeapType::kStringViewWtf8:
          case i::wasm::HeapType::kStringViewWtf16:
          case i::wasm::HeapType::kStringViewIter:
            values_out->set(index++, *Utils::OpenHandle(*value));
            break;
          case internal::wasm::HeapType::kBottom:
            UNREACHABLE();
          default:
            // TODO(7748): Add support for custom struct/array types.
            UNIMPLEMENTED();
        }
        break;
      case i::wasm::kRtt:
      case i::wasm::kI8:
      case i::wasm::kI16:
      case i::wasm::kVoid:
      case i::wasm::kBottom:
      case i::wasm::kS128:
        UNREACHABLE();
    }
  }
}

}  // namespace

void WebAssemblyException(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);

  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Exception()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Exception must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a WebAssembly tag");
    return;
  }
  i::Handle<i::Object> arg0 = Utils::OpenHandle(*args[0]);
  if (!i::HeapObject::cast(*arg0).IsWasmTagObject()) {
    thrower.TypeError("Argument 0 must be a WebAssembly tag");
    return;
  }
  i::Handle<i::WasmTagObject> tag_object =
      i::Handle<i::WasmTagObject>::cast(arg0);
  i::Handle<i::WasmExceptionTag> tag(
      i::WasmExceptionTag::cast(tag_object->tag()), i_isolate);
  uint32_t size = GetEncodedSize(tag_object);
  i::Handle<i::WasmExceptionPackage> runtime_exception =
      i::WasmExceptionPackage::New(i_isolate, tag, size);
  // The constructor above should guarantee that the cast below succeeds.
  i::Handle<i::FixedArray> values = i::Handle<i::FixedArray>::cast(
      i::WasmExceptionPackage::GetExceptionValues(i_isolate,
                                                  runtime_exception));
  i::Handle<i::PodArray<i::wasm::ValueType>> signature(
      tag_object->serialized_signature(), i_isolate);
  EncodeExceptionValues(isolate, signature, args[1], &thrower, values);
  if (thrower.error()) return;

  // Third argument: optional ExceptionOption ({traceStack: <bool>}).
  if (!args[2]->IsNullOrUndefined() && !args[2]->IsObject()) {
    thrower.TypeError("Argument 2 is not an object");
    return;
  }
  if (args[2]->IsObject()) {
    Local<Context> context = isolate->GetCurrentContext();
    Local<Object> trace_stack_obj = Local<Object>::Cast(args[2]);
    Local<String> trace_stack_key = v8_str(isolate, "traceStack");
    v8::MaybeLocal<v8::Value> maybe_trace_stack =
        trace_stack_obj->Get(context, trace_stack_key);
    v8::Local<Value> trace_stack_value;
    if (maybe_trace_stack.ToLocal(&trace_stack_value) &&
        trace_stack_value->BooleanValue(isolate)) {
      auto caller = Utils::OpenHandle(*args.NewTarget());
      i_isolate->CaptureAndSetErrorStack(runtime_exception, i::SKIP_NONE,
                                         caller);
      i::Handle<i::AccessorInfo> error_stack =
          i_isolate->factory()->error_stack_accessor();
      i::Handle<i::Name> name(i::Name::cast(error_stack->name()), i_isolate);
      i::JSObject::SetAccessor(runtime_exception, name, error_stack,
                               i::DONT_ENUM)
          .Assert();
    }
  }

  args.GetReturnValue().Set(
      Utils::ToLocal(i::Handle<i::Object>::cast(runtime_exception)));
}

namespace {
bool HasJSPromiseIntegrationFlag(Isolate* isolate, Local<Object> usage_obj,
                                 ErrorThrower* thrower, const char* flag_name) {
  Local<Context> context = isolate->GetCurrentContext();
  Local<String> flag_str = v8_str(isolate, flag_name);
  Local<String> first_str = v8_str(isolate, "first");
  Local<String> last_str = v8_str(isolate, "last");
  Local<String> none_str = v8_str(isolate, "none");
  v8::MaybeLocal<v8::Value> maybe_flag = usage_obj->Get(context, flag_str);
  v8::Local<Value> flag_value;
  v8::Local<String> flag_value_str;
  if (maybe_flag.ToLocal(&flag_value) && !flag_value->IsUndefined() &&
      flag_value->ToString(context).ToLocal(&flag_value_str)) {
    if (!flag_value_str->StringEquals(first_str) &&
        !flag_value_str->StringEquals(last_str) &&
        !flag_value_str->StringEquals(none_str)) {
      thrower->TypeError(
          "JS Promise Integration: Expected suspender "
          "position to be \"first\", \"last\" or \"none\"");
      return false;
    } else if (flag_value_str->StringEquals(last_str)) {
      // TODO(thibaudm): Support the "last" position.
      UNIMPLEMENTED();
    } else if (flag_value_str->StringEquals(first_str)) {
      return true;
    }
  }
  return false;
}

// Given {inner_sig}: [ti*] -> [to*]
// {outer_sig} must be: [externref ti*] -> [to*]
bool IsSuspendingSignature(const i::wasm::FunctionSig* inner_sig,
                           const i::wasm::FunctionSig* outer_sig) {
  if (inner_sig->parameter_count() + 1 != outer_sig->parameter_count()) {
    return false;
  }
  if (inner_sig->return_count() != outer_sig->return_count()) {
    return false;
  }
  if (outer_sig->GetParam(0) != i::wasm::kWasmExternRef) return false;
  for (size_t i = 1; i < outer_sig->parameter_count(); ++i) {
    if (outer_sig->GetParam(i) != inner_sig->GetParam(i - 1)) return false;
  }
  for (size_t i = 0; i < outer_sig->return_count(); ++i) {
    if (outer_sig->GetReturn(i) != inner_sig->GetReturn(i)) return false;
  }
  return true;
}

// Given {inner_sig}: externref [ti*] -> [to*]
// {outer_sig} must be: [ti*] -> [externref]
bool IsPromisingSignature(const i::wasm::FunctionSig* inner_sig,
                          const i::wasm::FunctionSig* outer_sig) {
  if (inner_sig->parameter_count() != outer_sig->parameter_count() + 1) {
    return false;
  }
  if (outer_sig->return_count() != 1) {
    return false;
  }
  if (inner_sig->GetParam(0) != i::wasm::kWasmExternRef) return false;
  for (size_t i = 0; i < outer_sig->parameter_count(); ++i) {
    if (outer_sig->GetParam(i) != inner_sig->GetParam(i + 1)) return false;
  }
  if (outer_sig->GetReturn(0) != i::wasm::kWasmExternRef) return false;
  return true;
}

}  // namespace

// WebAssembly.Function
void WebAssemblyFunction(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Function()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Function must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a function type");
    return;
  }
  Local<Object> function_type = Local<Object>::Cast(args[0]);
  Local<Context> context = isolate->GetCurrentContext();
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);

  // Load the 'parameters' property of the function type.
  Local<String> parameters_key = v8_str(isolate, "parameters");
  v8::MaybeLocal<v8::Value> parameters_maybe =
      function_type->Get(context, parameters_key);
  v8::Local<v8::Value> parameters_value;
  if (!parameters_maybe.ToLocal(&parameters_value) ||
      !parameters_value->IsObject()) {
    thrower.TypeError("Argument 0 must be a function type with 'parameters'");
    return;
  }
  Local<Object> parameters = parameters_value.As<Object>();
  uint32_t parameters_len = GetIterableLength(i_isolate, context, parameters);
  if (parameters_len == i::kMaxUInt32) {
    thrower.TypeError("Argument 0 contains parameters without 'length'");
    return;
  }
  if (parameters_len > i::wasm::kV8MaxWasmFunctionParams) {
    thrower.TypeError("Argument 0 contains too many parameters");
    return;
  }

  // Load the 'results' property of the function type.
  Local<String> results_key = v8_str(isolate, "results");
  v8::MaybeLocal<v8::Value> results_maybe =
      function_type->Get(context, results_key);
  v8::Local<v8::Value> results_value;
  if (!results_maybe.ToLocal(&results_value)) return;
  if (!results_value->IsObject()) {
    thrower.TypeError("Argument 0 must be a function type with 'results'");
    return;
  }
  Local<Object> results = results_value.As<Object>();
  uint32_t results_len = GetIterableLength(i_isolate, context, results);
  if (results_len == i::kMaxUInt32) {
    thrower.TypeError("Argument 0 contains results without 'length'");
    return;
  }
  if (results_len > i::wasm::kV8MaxWasmFunctionReturns) {
    thrower.TypeError("Argument 0 contains too many results");
    return;
  }

  // Decode the function type and construct a signature.
  i::Zone zone(i_isolate->allocator(), ZONE_NAME);
  i::wasm::FunctionSig::Builder builder(&zone, results_len, parameters_len);
  for (uint32_t i = 0; i < parameters_len; ++i) {
    i::wasm::ValueType type;
    MaybeLocal<Value> maybe = parameters->Get(context, i);
    if (!GetValueType(isolate, maybe, context, &type, enabled_features) ||
        type == i::wasm::kWasmVoid) {
      thrower.TypeError(
          "Argument 0 parameter type at index #%u must be a value type", i);
      return;
    }
    builder.AddParam(type);
  }
  for (uint32_t i = 0; i < results_len; ++i) {
    i::wasm::ValueType type;
    MaybeLocal<Value> maybe = results->Get(context, i);
    if (!GetValueType(isolate, maybe, context, &type, enabled_features)) return;
    if (type == i::wasm::kWasmVoid) {
      thrower.TypeError(
          "Argument 0 result type at index #%u must be a value type", i);
      return;
    }
    builder.AddReturn(type);
  }

  if (!args[1]->IsFunction()) {
    thrower.TypeError("Argument 1 must be a function");
    return;
  }
  const i::wasm::FunctionSig* sig = builder.Build();

  i::Handle<i::JSReceiver> callable =
      Utils::OpenHandle(*args[1].As<Function>());

  i::wasm::Suspend suspend = i::wasm::kNoSuspend;
  i::wasm::Promise promise = i::wasm::kNoPromise;
  if (i::v8_flags.experimental_wasm_stack_switching) {
    // Optional third argument for JS Promise Integration.
    if (!args[2]->IsNullOrUndefined() && !args[2]->IsObject()) {
      thrower.TypeError(
          "Expected argument 3 to be an object with a "
          "'suspending' or 'promising' property");
      return;
    }
    if (args[2]->IsObject()) {
      Local<Object> usage_obj = Local<Object>::Cast(args[2]);
      if (HasJSPromiseIntegrationFlag(isolate, usage_obj, &thrower,
                                      "suspending")) {
        suspend = i::wasm::kSuspend;
      }
      if (HasJSPromiseIntegrationFlag(isolate, usage_obj, &thrower,
                                      "promising")) {
        promise = i::wasm::kPromise;
      }
    }
  }

  bool is_wasm_exported_function =
      i::WasmExportedFunction::IsWasmExportedFunction(*callable);
  bool is_wasm_js_function = i::WasmJSFunction::IsWasmJSFunction(*callable);

  if (is_wasm_exported_function && !suspend && !promise) {
    if (*i::Handle<i::WasmExportedFunction>::cast(callable)->sig() == *sig) {
      args.GetReturnValue().Set(Utils::ToLocal(callable));
      return;
    }

    thrower.TypeError(
        "The signature of Argument 1 (a WebAssembly function) does "
        "not match the signature specified in Argument 0");
    return;
  }

  if (is_wasm_js_function && !suspend && !promise) {
    if (i::Handle<i::WasmJSFunction>::cast(callable)->MatchesSignature(sig)) {
      args.GetReturnValue().Set(Utils::ToLocal(callable));
      return;
    }

    thrower.TypeError(
        "The signature of Argument 1 (a WebAssembly function) does "
        "not match the signature specified in Argument 0");
    return;
  }

  if (is_wasm_exported_function && suspend) {
    // TODO(thibaudm): Support wasm-to-wasm calls with suspending behavior, and
    // also with combined promising+suspending behavior.
    UNIMPLEMENTED();
  }
  if (is_wasm_exported_function && promise) {
    auto wasm_exported_function = i::WasmExportedFunction::cast(*callable);
    i::SharedFunctionInfo sfi = wasm_exported_function.shared();
    i::WasmExportedFunctionData data = sfi.wasm_exported_function_data();
    if (!IsPromisingSignature(data.sig(), sig)) {
      thrower.TypeError("Incompatible signature for promising function");
      return;
    }
    i::Handle<i::WasmInstanceObject> instance(
        i::WasmInstanceObject::cast(data.internal().ref()), i_isolate);
    int func_index = data.function_index();
    i::Handle<i::CodeT> wrapper =
        BUILTIN_CODE(i_isolate, WasmReturnPromiseOnSuspend);
    i::Handle<i::JSFunction> result = i::WasmExportedFunction::New(
        i_isolate, instance, func_index,
        static_cast<int>(data.sig()->parameter_count()), wrapper);
    args.GetReturnValue().Set(Utils::ToLocal(result));
    return;
  }
  if (is_wasm_js_function && promise) {
    // TODO(thibaudm): This case has no practical use. The generated suspender
    // would be unusable since the stack would always contain at least one JS
    // frame. But for now the spec would require us to add specific JS-to-JS and
    // wasm-to-JS wrappers to support this case. Leave this unimplemented for
    // now.
    UNIMPLEMENTED();
  }
  if (is_wasm_js_function && suspend) {
    auto wasm_js_function = i::WasmJSFunction::cast(*callable);
    const i::wasm::FunctionSig* inner_sig =
        wasm_js_function.GetSignature(&zone);
    if (!IsSuspendingSignature(inner_sig, sig)) {
      thrower.TypeError("Incompatible signature for suspending function");
      return;
    }
  }
  i::Handle<i::JSFunction> result =
      i::WasmJSFunction::New(i_isolate, sig, callable, suspend);
  args.GetReturnValue().Set(Utils::ToLocal(result));
}

// WebAssembly.Function.type(WebAssembly.Function) -> FunctionType
void WebAssemblyFunctionType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Function.type()");

  const i::wasm::FunctionSig* sig;
  i::Zone zone(i_isolate->allocator(), ZONE_NAME);
  i::Handle<i::Object> arg0 = Utils::OpenHandle(*args[0]);
  if (i::WasmExportedFunction::IsWasmExportedFunction(*arg0)) {
    auto wasm_exported_function =
        i::Handle<i::WasmExportedFunction>::cast(arg0);
    auto sfi = handle(wasm_exported_function->shared(), i_isolate);
    i::Handle<i::WasmExportedFunctionData> data =
        handle(sfi->wasm_exported_function_data(), i_isolate);
    sig = wasm_exported_function->sig();
    if (i::WasmFunctionData::PromiseField::decode(data->js_promise_flags())) {
      // If this export is "promising", the first parameter of the original
      // function is an externref (suspender) which does not appear in the
      // wrapper function's signature. The wrapper function also returns a
      // promise as an externref instead of the original return type.
      size_t param_count = sig->parameter_count();
      DCHECK_GE(param_count, 1);
      DCHECK_EQ(sig->GetParam(0), i::wasm::kWasmExternRef);
      i::wasm::FunctionSig::Builder builder(&zone, 1, param_count - 1);
      for (size_t i = 1; i < param_count; ++i) {
        builder.AddParam(sig->GetParam(i));
      }
      builder.AddReturn(i::wasm::kWasmExternRef);
      sig = builder.Build();
    }
  } else if (i::WasmJSFunction::IsWasmJSFunction(*arg0)) {
    sig = i::Handle<i::WasmJSFunction>::cast(arg0)->GetSignature(&zone);
  } else {
    thrower.TypeError("Argument 0 must be a WebAssembly.Function");
    return;
  }

  auto type = i::wasm::GetTypeForFunction(i_isolate, sig);
  args.GetReturnValue().Set(Utils::ToLocal(type));
}

constexpr const char* kName_WasmGlobalObject = "WebAssembly.Global";
constexpr const char* kName_WasmMemoryObject = "WebAssembly.Memory";
constexpr const char* kName_WasmInstanceObject = "WebAssembly.Instance";
constexpr const char* kName_WasmTableObject = "WebAssembly.Table";
constexpr const char* kName_WasmTagObject = "WebAssembly.Tag";
constexpr const char* kName_WasmExceptionPackage = "WebAssembly.Exception";

#define EXTRACT_THIS(var, WasmType)                                  \
  i::Handle<i::WasmType> var;                                        \
  {                                                                  \
    i::Handle<i::Object> this_arg = Utils::OpenHandle(*args.This()); \
    if (!this_arg->Is##WasmType()) {                                 \
      thrower.TypeError("Receiver is not a %s", kName_##WasmType);   \
      return;                                                        \
    }                                                                \
    var = i::Handle<i::WasmType>::cast(this_arg);                    \
  }

void WebAssemblyInstanceGetExports(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Instance.exports()");
  EXTRACT_THIS(receiver, WasmInstanceObject);
  i::Handle<i::JSObject> exports_object(receiver->exports_object(), i_isolate);
  args.GetReturnValue().Set(Utils::ToLocal(exports_object));
}

void WebAssemblyTableGetLength(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.length()");
  EXTRACT_THIS(receiver, WasmTableObject);
  args.GetReturnValue().Set(
      v8::Number::New(isolate, receiver->current_length()));
}

// WebAssembly.Table.grow(num, init_value = null) -> num
void WebAssemblyTableGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.grow()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmTableObject);

  uint32_t grow_by;
  if (!EnforceUint32("Argument 0", args[0], context, &thrower, &grow_by)) {
    return;
  }

  i::Handle<i::Object> init_value;

  if (args.Length() >= 2 && !args[1]->IsUndefined()) {
    init_value = Utils::OpenHandle(*args[1]);
    const char* error_message;
    if (!i::WasmTableObject::JSToWasmElement(i_isolate, receiver, init_value,
                                             &error_message)
             .ToHandle(&init_value)) {
      thrower.TypeError("Argument 1 is invalid: %s", error_message);
      return;
    }
  } else if (receiver->type().is_non_nullable()) {
    thrower.TypeError(
        "Argument 1 must be specified for non-nullable element type");
    return;
  } else {
    init_value = DefaultReferenceValue(i_isolate, receiver->type());
  }

  int old_size =
      i::WasmTableObject::Grow(i_isolate, receiver, grow_by, init_value);
  if (old_size < 0) {
    thrower.RangeError("failed to grow table by %u", grow_by);
    return;
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(old_size);
}

namespace {
void WasmObjectToJSReturnValue(v8::ReturnValue<v8::Value>& return_value,
                               i::Handle<i::Object> value,
                               i::wasm::HeapType::Representation repr,
                               const i::wasm::WasmModule* module,
                               i::Isolate* isolate,
                               ScheduledErrorThrower* thrower) {
  switch (repr) {
    case i::wasm::HeapType::kExtern:
    case i::wasm::HeapType::kString:
    // TODO(7748): Make sure i31ref is compatible with Smi, or transform here.
    case i::wasm::HeapType::kI31:
      return_value.Set(Utils::ToLocal(value));
      return;
    case i::wasm::HeapType::kFunc: {
      if (!value->IsNull()) {
        DCHECK(value->IsWasmInternalFunction());
        value =
            handle(i::Handle<i::WasmInternalFunction>::cast(value)->external(),
                   isolate);
      }
      return_value.Set(Utils::ToLocal(value));
      return;
    }
    case i::wasm::HeapType::kStringViewWtf8:
      thrower->TypeError("stringview_wtf8 has no JS representation");
      return;
    case i::wasm::HeapType::kStringViewWtf16:
      thrower->TypeError("stringview_wtf16 has no JS representation");
      return;
    case i::wasm::HeapType::kStringViewIter:
      thrower->TypeError("stringview_iter has no JS representation");
      return;
    case i::wasm::HeapType::kBottom:
      UNREACHABLE();
    case i::wasm::HeapType::kData:
    case i::wasm::HeapType::kArray:
    case i::wasm::HeapType::kEq:
    case i::wasm::HeapType::kAny: {
      if (!i::v8_flags.wasm_gc_js_interop && value->IsWasmObject()) {
        // Transform wasm object into JS-compliant representation.
        i::Handle<i::JSObject> wrapper =
            isolate->factory()->NewJSObject(isolate->object_function());
        i::JSObject::AddProperty(
            isolate, wrapper, isolate->factory()->wasm_wrapped_object_symbol(),
            value, i::NONE);
        value = wrapper;
      }
      return_value.Set(Utils::ToLocal(value));
      return;
    }
    default:
      if (module->has_signature(repr)) {
        if (!value->IsNull()) {
          DCHECK(value->IsWasmInternalFunction());
          value = handle(
              i::Handle<i::WasmInternalFunction>::cast(value)->external(),
              isolate);
        }
        return_value.Set(Utils::ToLocal(value));
        return;
      }
      if (!i::v8_flags.wasm_gc_js_interop && value->IsWasmObject()) {
        // Transform wasm object into JS-compliant representation.
        i::Handle<i::JSObject> wrapper =
            isolate->factory()->NewJSObject(isolate->object_function());
        i::JSObject::AddProperty(
            isolate, wrapper, isolate->factory()->wasm_wrapped_object_symbol(),
            value, i::NONE);
        value = wrapper;
      }
      return_value.Set(Utils::ToLocal(value));
      return;
  }
}
}  // namespace

// WebAssembly.Table.get(num) -> any
void WebAssemblyTableGet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.get()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmTableObject);

  uint32_t index;
  if (!EnforceUint32("Argument 0", args[0], context, &thrower, &index)) {
    return;
  }
  if (!i::WasmTableObject::IsInBounds(i_isolate, receiver, index)) {
    thrower.RangeError("invalid index %u into function table", index);
    return;
  }

  if (receiver->type() == i::wasm::kWasmStringViewWtf8) {
    thrower.TypeError("stringview_wtf8 has no JS representation");
    return;
  }
  if (receiver->type() == i::wasm::kWasmStringViewWtf16) {
    thrower.TypeError("stringview_wtf16 has no JS representation");
    return;
  }
  if (receiver->type() == i::wasm::kWasmStringViewIter) {
    thrower.TypeError("stringview_iter has no JS representation");
    return;
  }

  i::Handle<i::Object> result =
      i::WasmTableObject::Get(i_isolate, receiver, index);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  const i::wasm::WasmModule* module =
      receiver->instance().IsWasmInstanceObject()
          ? i::WasmInstanceObject::cast(receiver->instance()).module()
          : nullptr;
  WasmObjectToJSReturnValue(return_value, result,
                            receiver->type().heap_representation(), module,
                            i_isolate, &thrower);
}

// WebAssembly.Table.set(num, any)
void WebAssemblyTableSet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.set()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(table_object, WasmTableObject);

  // Parameter 0.
  uint32_t index;
  if (!EnforceUint32("Argument 0", args[0], context, &thrower, &index)) {
    return;
  }
  if (!i::WasmTableObject::IsInBounds(i_isolate, table_object, index)) {
    thrower.RangeError("invalid index %u into function table", index);
    return;
  }

  i::Handle<i::Object> element;
  if (args.Length() >= 2) {
    element = Utils::OpenHandle(*args[1]);
  } else if (table_object->type().is_defaultable()) {
    element = DefaultReferenceValue(i_isolate, table_object->type());
  } else {
    thrower.TypeError("Table of non-defaultable type %s needs explicit element",
                      table_object->type().name().c_str());
    return;
  }

  const char* error_message;
  if (!i::WasmTableObject::JSToWasmElement(i_isolate, table_object, element,
                                           &error_message)
           .ToHandle(&element)) {
    thrower.TypeError("Argument 1 is invalid for table: %s", error_message);
    return;
  }

  i::WasmTableObject::Set(i_isolate, table_object, index, element);
}

// WebAssembly.Table.type() -> TableType
void WebAssemblyTableType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.type()");

  EXTRACT_THIS(table, WasmTableObject);
  base::Optional<uint32_t> max_size;
  if (!table->maximum_length().IsUndefined()) {
    uint64_t max_size64 = table->maximum_length().Number();
    DCHECK_LE(max_size64, std::numeric_limits<uint32_t>::max());
    max_size.emplace(static_cast<uint32_t>(max_size64));
  }
  auto type = i::wasm::GetTypeForTable(i_isolate, table->type(),
                                       table->current_length(), max_size);
  args.GetReturnValue().Set(Utils::ToLocal(type));
}

// WebAssembly.Memory.grow(num) -> num
void WebAssemblyMemoryGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory.grow()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmMemoryObject);

  uint32_t delta_pages;
  if (!EnforceUint32("Argument 0", args[0], context, &thrower, &delta_pages)) {
    return;
  }

  i::Handle<i::JSArrayBuffer> old_buffer(receiver->array_buffer(), i_isolate);

  uint64_t old_pages64 = old_buffer->byte_length() / i::wasm::kWasmPageSize;
  uint64_t new_pages64 = old_pages64 + static_cast<uint64_t>(delta_pages);

  if (new_pages64 > static_cast<uint64_t>(receiver->maximum_pages())) {
    thrower.RangeError("Maximum memory size exceeded");
    return;
  }

  int32_t ret = i::WasmMemoryObject::Grow(i_isolate, receiver, delta_pages);
  if (ret == -1) {
    thrower.RangeError("Unable to grow instance memory");
    return;
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
}

// WebAssembly.Memory.buffer -> ArrayBuffer
void WebAssemblyMemoryGetBuffer(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory.buffer");
  EXTRACT_THIS(receiver, WasmMemoryObject);

  i::Handle<i::Object> buffer_obj(receiver->array_buffer(), i_isolate);
  DCHECK(buffer_obj->IsJSArrayBuffer());
  i::Handle<i::JSArrayBuffer> buffer(i::JSArrayBuffer::cast(*buffer_obj),
                                     i_isolate);
  if (buffer->is_shared()) {
    // TODO(gdeepti): More needed here for when cached buffer, and current
    // buffer are out of sync, handle that here when bounds checks, and Grow
    // are handled correctly.
    Maybe<bool> result =
        buffer->SetIntegrityLevel(buffer, i::FROZEN, i::kDontThrow);
    if (!result.FromJust()) {
      thrower.TypeError(
          "Status of setting SetIntegrityLevel of buffer is false.");
    }
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(buffer));
}

// WebAssembly.Memory.type() -> MemoryType
void WebAssemblyMemoryType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory.type()");

  EXTRACT_THIS(memory, WasmMemoryObject);
  i::Handle<i::JSArrayBuffer> buffer(memory->array_buffer(), i_isolate);
  size_t curr_size = buffer->byte_length() / i::wasm::kWasmPageSize;
  DCHECK_LE(curr_size, std::numeric_limits<uint32_t>::max());
  uint32_t min_size = static_cast<uint32_t>(curr_size);
  base::Optional<uint32_t> max_size;
  if (memory->has_maximum_pages()) {
    uint64_t max_size64 = memory->maximum_pages();
    DCHECK_LE(max_size64, std::numeric_limits<uint32_t>::max());
    max_size.emplace(static_cast<uint32_t>(max_size64));
  }
  bool shared = buffer->is_shared();
  auto type = i::wasm::GetTypeForMemory(i_isolate, min_size, max_size, shared);
  args.GetReturnValue().Set(Utils::ToLocal(type));
}

// WebAssembly.Tag.type() -> FunctionType
void WebAssemblyTagType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Tag.type()");

  EXTRACT_THIS(tag, WasmTagObject);
  if (thrower.error()) return;

  int n = tag->serialized_signature().length();
  std::vector<i::wasm::ValueType> data(n);
  if (n > 0) {
    tag->serialized_signature().copy_out(0, data.data(), n);
  }
  const i::wasm::FunctionSig sig{0, data.size(), data.data()};
  constexpr bool kForException = true;
  auto type = i::wasm::GetTypeForFunction(i_isolate, &sig, kForException);
  args.GetReturnValue().Set(Utils::ToLocal(type));
}

void WebAssemblyExceptionGetArg(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Exception.getArg()");

  EXTRACT_THIS(exception, WasmExceptionPackage);
  if (thrower.error()) return;

  i::MaybeHandle<i::WasmTagObject> maybe_tag =
      GetFirstArgumentAsTag(args, &thrower);
  if (thrower.error()) return;
  auto tag = maybe_tag.ToHandleChecked();
  Local<Context> context = isolate->GetCurrentContext();
  uint32_t index;
  if (!EnforceUint32("Index", args[1], context, &thrower, &index)) {
    return;
  }
  auto maybe_values =
      i::WasmExceptionPackage::GetExceptionValues(i_isolate, exception);

  auto this_tag =
      i::WasmExceptionPackage::GetExceptionTag(i_isolate, exception);
  DCHECK(this_tag->IsWasmExceptionTag());
  if (tag->tag() != *this_tag) {
    thrower.TypeError("First argument does not match the exception tag");
    return;
  }

  DCHECK(!maybe_values->IsUndefined());
  auto values = i::Handle<i::FixedArray>::cast(maybe_values);
  auto signature = tag->serialized_signature();
  if (index >= static_cast<uint32_t>(signature.length())) {
    thrower.RangeError("Index out of range");
    return;
  }
  // First, find the index in the values array.
  uint32_t decode_index = 0;
  // Since the bounds check above passed, the cast to int is safe.
  for (int i = 0; i < static_cast<int>(index); ++i) {
    switch (signature.get(i).kind()) {
      case i::wasm::kI32:
      case i::wasm::kF32:
        decode_index += 2;
        break;
      case i::wasm::kI64:
      case i::wasm::kF64:
        decode_index += 4;
        break;
      case i::wasm::kRef:
      case i::wasm::kRefNull:
        switch (signature.get(i).heap_representation()) {
          case i::wasm::HeapType::kFunc:
          case i::wasm::HeapType::kExtern:
          case i::wasm::HeapType::kAny:
          case i::wasm::HeapType::kEq:
          case i::wasm::HeapType::kI31:
          case i::wasm::HeapType::kData:
          case i::wasm::HeapType::kArray:
          case i::wasm::HeapType::kString:
          case i::wasm::HeapType::kStringViewWtf8:
          case i::wasm::HeapType::kStringViewWtf16:
          case i::wasm::HeapType::kStringViewIter:
            decode_index++;
            break;
          case i::wasm::HeapType::kBottom:
            UNREACHABLE();
          default:
            // TODO(7748): Add support for custom struct/array types.
            UNIMPLEMENTED();
        }
        break;
      case i::wasm::kRtt:
      case i::wasm::kI8:
      case i::wasm::kI16:
      case i::wasm::kVoid:
      case i::wasm::kBottom:
      case i::wasm::kS128:
        UNREACHABLE();
    }
  }
  // Decode the value at {decode_index}.
  Local<Value> result;
  switch (signature.get(index).kind()) {
    case i::wasm::kI32: {
      uint32_t u32_bits = 0;
      i::DecodeI32ExceptionValue(values, &decode_index, &u32_bits);
      int32_t i32 = static_cast<int32_t>(u32_bits);
      result = v8::Integer::New(isolate, i32);
      break;
    }
    case i::wasm::kI64: {
      uint64_t u64_bits = 0;
      i::DecodeI64ExceptionValue(values, &decode_index, &u64_bits);
      int64_t i64 = static_cast<int64_t>(u64_bits);
      result = v8::BigInt::New(isolate, i64);
      break;
    }
    case i::wasm::kF32: {
      uint32_t f32_bits = 0;
      DecodeI32ExceptionValue(values, &decode_index, &f32_bits);
      float f32 = base::bit_cast<float>(f32_bits);
      result = v8::Number::New(isolate, f32);
      break;
    }
    case i::wasm::kF64: {
      uint64_t f64_bits = 0;
      DecodeI64ExceptionValue(values, &decode_index, &f64_bits);
      double f64 = base::bit_cast<double>(f64_bits);
      result = v8::Number::New(isolate, f64);
      break;
    }
    case i::wasm::kRef:
    case i::wasm::kRefNull:
      switch (signature.get(index).heap_representation()) {
        case i::wasm::HeapType::kFunc:
        case i::wasm::HeapType::kExtern:
        case i::wasm::HeapType::kAny:
        case i::wasm::HeapType::kEq:
        case i::wasm::HeapType::kI31:
        case i::wasm::HeapType::kArray:
        case i::wasm::HeapType::kData:
        case i::wasm::HeapType::kString:
        case i::wasm::HeapType::kStringViewWtf8:
        case i::wasm::HeapType::kStringViewWtf16:
        case i::wasm::HeapType::kStringViewIter: {
          auto obj = values->get(decode_index);
          result = Utils::ToLocal(i::Handle<i::Object>(obj, i_isolate));
          break;
        }
        case i::wasm::HeapType::kBottom:
          UNREACHABLE();
        default:
          // TODO(7748): Add support for custom struct/array types.
          UNIMPLEMENTED();
      }
      break;
    case i::wasm::kRtt:
    case i::wasm::kI8:
    case i::wasm::kI16:
    case i::wasm::kVoid:
    case i::wasm::kBottom:
    case i::wasm::kS128:
      UNREACHABLE();
  }
  args.GetReturnValue().Set(result);
}

void WebAssemblyExceptionIs(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Exception.is()");

  EXTRACT_THIS(exception, WasmExceptionPackage);
  if (thrower.error()) return;

  auto tag = i::WasmExceptionPackage::GetExceptionTag(i_isolate, exception);
  DCHECK(tag->IsWasmExceptionTag());

  auto maybe_tag = GetFirstArgumentAsTag(args, &thrower);
  if (thrower.error()) {
    return;
  }
  auto tag_arg = maybe_tag.ToHandleChecked();
  args.GetReturnValue().Set(tag_arg->tag() == *tag);
}

void WebAssemblyGlobalGetValueCommon(
    const v8::FunctionCallbackInfo<v8::Value>& args, const char* name) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, name);
  EXTRACT_THIS(receiver, WasmGlobalObject);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();

  switch (receiver->type().kind()) {
    case i::wasm::kI32:
      return_value.Set(receiver->GetI32());
      break;
    case i::wasm::kI64: {
      Local<BigInt> value = BigInt::New(isolate, receiver->GetI64());
      return_value.Set(value);
      break;
    }
    case i::wasm::kF32:
      return_value.Set(receiver->GetF32());
      break;
    case i::wasm::kF64:
      return_value.Set(receiver->GetF64());
      break;
    case i::wasm::kS128:
      thrower.TypeError("Can't get the value of s128 WebAssembly.Global");
      break;
    case i::wasm::kRef:
    case i::wasm::kRefNull: {
      const i::wasm::WasmModule* module =
          receiver->instance().IsWasmInstanceObject()
              ? i::WasmInstanceObject::cast(receiver->instance()).module()
              : nullptr;
      WasmObjectToJSReturnValue(return_value, receiver->GetRef(),
                                receiver->type().heap_representation(), module,
                                i_isolate, &thrower);
      break;
    }
    case i::wasm::kRtt:
      UNIMPLEMENTED();  // TODO(7748): Implement.
    case i::wasm::kI8:
    case i::wasm::kI16:
    case i::wasm::kBottom:
    case i::wasm::kVoid:
      UNREACHABLE();
  }
}

// WebAssembly.Global.valueOf() -> num
void WebAssemblyGlobalValueOf(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WebAssemblyGlobalGetValueCommon(args, "WebAssembly.Global.valueOf()");
}

// get WebAssembly.Global.value -> num
void WebAssemblyGlobalGetValue(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WebAssemblyGlobalGetValueCommon(args, "get WebAssembly.Global.value");
}

// set WebAssembly.Global.value(num)
void WebAssemblyGlobalSetValue(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  ScheduledErrorThrower thrower(i_isolate, "set WebAssembly.Global.value");
  EXTRACT_THIS(receiver, WasmGlobalObject);

  if (!receiver->is_mutable()) {
    thrower.TypeError("Can't set the value of an immutable global.");
    return;
  }
  if (args.Length() == 0) {
    thrower.TypeError("Argument 0 is required");
    return;
  }

  switch (receiver->type().kind()) {
    case i::wasm::kI32: {
      int32_t i32_value = 0;
      if (!args[0]->Int32Value(context).To(&i32_value)) return;
      receiver->SetI32(i32_value);
      break;
    }
    case i::wasm::kI64: {
      v8::Local<v8::BigInt> bigint_value;
      if (!args[0]->ToBigInt(context).ToLocal(&bigint_value)) return;
      receiver->SetI64(bigint_value->Int64Value());
      break;
    }
    case i::wasm::kF32: {
      double f64_value = 0;
      if (!args[0]->NumberValue(context).To(&f64_value)) return;
      receiver->SetF32(i::DoubleToFloat32(f64_value));
      break;
    }
    case i::wasm::kF64: {
      double f64_value = 0;
      if (!args[0]->NumberValue(context).To(&f64_value)) return;
      receiver->SetF64(f64_value);
      break;
    }
    case i::wasm::kS128:
      thrower.TypeError("Can't set the value of s128 WebAssembly.Global");
      break;
    case i::wasm::kRef:
    case i::wasm::kRefNull: {
      const i::wasm::WasmModule* module =
          receiver->instance().IsWasmInstanceObject()
              ? i::WasmInstanceObject::cast(receiver->instance()).module()
              : nullptr;
      i::Handle<i::Object> value = Utils::OpenHandle(*args[0]);
      const char* error_message;
      if (!i::wasm::JSToWasmObject(i_isolate, module, value, receiver->type(),
                                   &error_message)
               .ToHandle(&value)) {
        thrower.TypeError("%s", error_message);
        return;
      }
      receiver->SetRef(value);
      return;
    }
    case i::wasm::kRtt:
    case i::wasm::kI8:
    case i::wasm::kI16:
    case i::wasm::kBottom:
    case i::wasm::kVoid:
      UNREACHABLE();
  }
}

// WebAssembly.Global.type() -> GlobalType
void WebAssemblyGlobalType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Global.type()");

  EXTRACT_THIS(global, WasmGlobalObject);
  auto type = i::wasm::GetTypeForGlobal(i_isolate, global->is_mutable(),
                                        global->type());
  args.GetReturnValue().Set(Utils::ToLocal(type));
}

}  // namespace

// TODO(titzer): we use the API to create the function template because the
// internal guts are too ugly to replicate here.
static i::Handle<i::FunctionTemplateInfo> NewFunctionTemplate(
    i::Isolate* i_isolate, FunctionCallback func, bool has_prototype,
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect) {
  Isolate* isolate = reinterpret_cast<Isolate*>(i_isolate);
  ConstructorBehavior behavior =
      has_prototype ? ConstructorBehavior::kAllow : ConstructorBehavior::kThrow;
  Local<FunctionTemplate> templ = FunctionTemplate::New(
      isolate, func, {}, {}, 0, behavior, side_effect_type);
  if (has_prototype) templ->ReadOnlyPrototype();
  return v8::Utils::OpenHandle(*templ);
}

static i::Handle<i::ObjectTemplateInfo> NewObjectTemplate(
    i::Isolate* i_isolate) {
  Isolate* isolate = reinterpret_cast<Isolate*>(i_isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  return v8::Utils::OpenHandle(*templ);
}

namespace internal {

Handle<JSFunction> CreateFunc(
    Isolate* isolate, Handle<String> name, FunctionCallback func,
    bool has_prototype,
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect) {
  Handle<FunctionTemplateInfo> temp =
      NewFunctionTemplate(isolate, func, has_prototype, side_effect_type);
  Handle<JSFunction> function =
      ApiNatives::InstantiateFunction(temp, name).ToHandleChecked();
  DCHECK(function->shared().HasSharedName());
  return function;
}

Handle<JSFunction> InstallFunc(
    Isolate* isolate, Handle<JSObject> object, const char* str,
    FunctionCallback func, int length, bool has_prototype = false,
    PropertyAttributes attributes = NONE,
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> function =
      CreateFunc(isolate, name, func, has_prototype, side_effect_type);
  function->shared().set_length(length);
  JSObject::AddProperty(isolate, object, name, function, attributes);
  return function;
}

Handle<JSFunction> InstallConstructorFunc(Isolate* isolate,
                                          Handle<JSObject> object,
                                          const char* str,
                                          FunctionCallback func) {
  return InstallFunc(isolate, object, str, func, 1, true, DONT_ENUM,
                     SideEffectType::kHasNoSideEffect);
}

Handle<String> GetterName(Isolate* isolate, Handle<String> name) {
  return Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
      .ToHandleChecked();
}

void InstallGetter(Isolate* isolate, Handle<JSObject> object, const char* str,
                   FunctionCallback func) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> function =
      CreateFunc(isolate, GetterName(isolate, name), func, false,
                 SideEffectType::kHasNoSideEffect);

  Utils::ToLocal(object)->SetAccessorProperty(Utils::ToLocal(name),
                                              Utils::ToLocal(function),
                                              Local<Function>(), v8::None);
}

Handle<String> SetterName(Isolate* isolate, Handle<String> name) {
  return Name::ToFunctionName(isolate, name, isolate->factory()->set_string())
      .ToHandleChecked();
}

void InstallGetterSetter(Isolate* isolate, Handle<JSObject> object,
                         const char* str, FunctionCallback getter,
                         FunctionCallback setter) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> getter_func =
      CreateFunc(isolate, GetterName(isolate, name), getter, false,
                 SideEffectType::kHasNoSideEffect);
  Handle<JSFunction> setter_func =
      CreateFunc(isolate, SetterName(isolate, name), setter, false);
  setter_func->shared().set_length(1);

  Utils::ToLocal(object)->SetAccessorProperty(
      Utils::ToLocal(name), Utils::ToLocal(getter_func),
      Utils::ToLocal(setter_func), v8::None);
}

// Assigns a dummy instance template to the given constructor function. Used to
// make sure the implicit receivers for the constructors in this file have an
// instance type different from the internal one, they allocate the resulting
// object explicitly and ignore implicit receiver.
void SetDummyInstanceTemplate(Isolate* isolate, Handle<JSFunction> fun) {
  Handle<ObjectTemplateInfo> instance_template = NewObjectTemplate(isolate);
  FunctionTemplateInfo::SetInstanceTemplate(
      isolate, handle(fun->shared().get_api_func_data(), isolate),
      instance_template);
}

Handle<JSObject> SetupConstructor(Isolate* isolate,
                                  Handle<JSFunction> constructor,
                                  InstanceType instance_type, int instance_size,
                                  const char* name = nullptr) {
  SetDummyInstanceTemplate(isolate, constructor);
  JSFunction::EnsureHasInitialMap(constructor);
  Handle<JSObject> proto(JSObject::cast(constructor->instance_prototype()),
                         isolate);
  Handle<Map> map = isolate->factory()->NewMap(instance_type, instance_size);
  JSFunction::SetInitialMap(isolate, constructor, map, proto);
  constexpr PropertyAttributes ro_attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
  if (name) {
    JSObject::AddProperty(isolate, proto,
                          isolate->factory()->to_string_tag_symbol(),
                          v8_str(isolate, name), ro_attributes);
  }
  return proto;
}

// static
void WasmJs::Install(Isolate* isolate, bool exposed_on_global_object) {
  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<Context> context(global->native_context(), isolate);
  // Install the JS API once only.
  Object prev = context->get(Context::WASM_MODULE_CONSTRUCTOR_INDEX);
  if (!prev.IsUndefined(isolate)) {
    DCHECK(prev.IsJSFunction());
    return;
  }

  Factory* factory = isolate->factory();

  // Setup WebAssembly
  Handle<String> name = v8_str(isolate, "WebAssembly");
  // Not supposed to be called, hence using the kIllegal builtin as code.
  Handle<SharedFunctionInfo> info =
      factory->NewSharedFunctionInfoForBuiltin(name, Builtin::kIllegal);
  info->set_language_mode(LanguageMode::kStrict);

  Handle<JSFunction> cons =
      Factory::JSFunctionBuilder{isolate, info, context}.Build();
  JSFunction::SetPrototype(cons, isolate->initial_object_prototype());
  Handle<JSObject> webassembly =
      factory->NewJSObject(cons, AllocationType::kOld);

  PropertyAttributes ro_attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
  JSObject::AddProperty(isolate, webassembly, factory->to_string_tag_symbol(),
                        name, ro_attributes);
  InstallFunc(isolate, webassembly, "compile", WebAssemblyCompile, 1);
  InstallFunc(isolate, webassembly, "validate", WebAssemblyValidate, 1);
  InstallFunc(isolate, webassembly, "instantiate", WebAssemblyInstantiate, 1);

  // TODO(tebbi): Put this behind its own flag once --wasm-gc-js-interop gets
  // closer to shipping.
  if (v8_flags.wasm_gc_js_interop) {
    SimpleInstallFunction(
        isolate, webassembly, "experimentalConvertArrayToString",
        Builtin::kExperimentalWasmConvertArrayToString, 0, true);
    SimpleInstallFunction(
        isolate, webassembly, "experimentalConvertStringToArray",
        Builtin::kExperimentalWasmConvertStringToArray, 0, true);
  }

  if (v8_flags.wasm_test_streaming) {
    isolate->set_wasm_streaming_callback(WasmStreamingCallbackForTesting);
  }

  if (isolate->wasm_streaming_callback() != nullptr) {
    InstallFunc(isolate, webassembly, "compileStreaming",
                WebAssemblyCompileStreaming, 1);
    InstallFunc(isolate, webassembly, "instantiateStreaming",
                WebAssemblyInstantiateStreaming, 1);
  }

  // Expose the API on the global object if configured to do so.
  if (exposed_on_global_object) {
    JSObject::AddProperty(isolate, global, name, webassembly, DONT_ENUM);
  }

  // Setup Module
  Handle<JSFunction> module_constructor =
      InstallConstructorFunc(isolate, webassembly, "Module", WebAssemblyModule);
  SetupConstructor(isolate, module_constructor, i::WASM_MODULE_OBJECT_TYPE,
                   WasmModuleObject::kHeaderSize, "WebAssembly.Module");
  context->set_wasm_module_constructor(*module_constructor);
  InstallFunc(isolate, module_constructor, "imports", WebAssemblyModuleImports,
              1, false, NONE, SideEffectType::kHasNoSideEffect);
  InstallFunc(isolate, module_constructor, "exports", WebAssemblyModuleExports,
              1, false, NONE, SideEffectType::kHasNoSideEffect);
  InstallFunc(isolate, module_constructor, "customSections",
              WebAssemblyModuleCustomSections, 2, false, NONE,
              SideEffectType::kHasNoSideEffect);

  // Setup Instance
  Handle<JSFunction> instance_constructor = InstallConstructorFunc(
      isolate, webassembly, "Instance", WebAssemblyInstance);
  Handle<JSObject> instance_proto = SetupConstructor(
      isolate, instance_constructor, i::WASM_INSTANCE_OBJECT_TYPE,
      WasmInstanceObject::kHeaderSize, "WebAssembly.Instance");
  context->set_wasm_instance_constructor(*instance_constructor);
  InstallGetter(isolate, instance_proto, "exports",
                WebAssemblyInstanceGetExports);

  // The context is not set up completely yet. That's why we cannot use
  // {WasmFeatures::FromIsolate} and have to use {WasmFeatures::FromFlags}
  // instead.
  auto enabled_features = i::wasm::WasmFeatures::FromFlags();

  // Setup Table
  Handle<JSFunction> table_constructor =
      InstallConstructorFunc(isolate, webassembly, "Table", WebAssemblyTable);
  Handle<JSObject> table_proto =
      SetupConstructor(isolate, table_constructor, i::WASM_TABLE_OBJECT_TYPE,
                       WasmTableObject::kHeaderSize, "WebAssembly.Table");
  context->set_wasm_table_constructor(*table_constructor);
  InstallGetter(isolate, table_proto, "length", WebAssemblyTableGetLength);
  InstallFunc(isolate, table_proto, "grow", WebAssemblyTableGrow, 1);
  InstallFunc(isolate, table_proto, "set", WebAssemblyTableSet, 1);
  InstallFunc(isolate, table_proto, "get", WebAssemblyTableGet, 1, false, NONE,
              SideEffectType::kHasNoSideEffect);
  if (enabled_features.has_type_reflection()) {
    InstallFunc(isolate, table_proto, "type", WebAssemblyTableType, 0, false,
                NONE, SideEffectType::kHasNoSideEffect);
  }

  // Setup Memory
  Handle<JSFunction> memory_constructor =
      InstallConstructorFunc(isolate, webassembly, "Memory", WebAssemblyMemory);
  Handle<JSObject> memory_proto =
      SetupConstructor(isolate, memory_constructor, i::WASM_MEMORY_OBJECT_TYPE,
                       WasmMemoryObject::kHeaderSize, "WebAssembly.Memory");
  context->set_wasm_memory_constructor(*memory_constructor);
  InstallFunc(isolate, memory_proto, "grow", WebAssemblyMemoryGrow, 1);
  InstallGetter(isolate, memory_proto, "buffer", WebAssemblyMemoryGetBuffer);
  if (enabled_features.has_type_reflection()) {
    InstallFunc(isolate, memory_proto, "type", WebAssemblyMemoryType, 0, false,
                NONE, SideEffectType::kHasNoSideEffect);
  }

  // Setup Global
  Handle<JSFunction> global_constructor =
      InstallConstructorFunc(isolate, webassembly, "Global", WebAssemblyGlobal);
  Handle<JSObject> global_proto =
      SetupConstructor(isolate, global_constructor, i::WASM_GLOBAL_OBJECT_TYPE,
                       WasmGlobalObject::kHeaderSize, "WebAssembly.Global");
  context->set_wasm_global_constructor(*global_constructor);
  InstallFunc(isolate, global_proto, "valueOf", WebAssemblyGlobalValueOf, 0,
              false, NONE, SideEffectType::kHasNoSideEffect);
  InstallGetterSetter(isolate, global_proto, "value", WebAssemblyGlobalGetValue,
                      WebAssemblyGlobalSetValue);
  if (enabled_features.has_type_reflection()) {
    InstallFunc(isolate, global_proto, "type", WebAssemblyGlobalType, 0, false,
                NONE, SideEffectType::kHasNoSideEffect);
  }

  // Setup Exception
  if (enabled_features.has_eh()) {
    Handle<JSFunction> tag_constructor =
        InstallConstructorFunc(isolate, webassembly, "Tag", WebAssemblyTag);
    Handle<JSObject> tag_proto =
        SetupConstructor(isolate, tag_constructor, i::WASM_TAG_OBJECT_TYPE,
                         WasmTagObject::kHeaderSize, "WebAssembly.Tag");
    context->set_wasm_tag_constructor(*tag_constructor);

    if (enabled_features.has_type_reflection()) {
      InstallFunc(isolate, tag_proto, "type", WebAssemblyTagType, 0);
    }
    // Set up runtime exception constructor.
    Handle<JSFunction> exception_constructor = InstallConstructorFunc(
        isolate, webassembly, "Exception", WebAssemblyException);
    SetDummyInstanceTemplate(isolate, exception_constructor);
    Handle<JSObject> exception_proto = SetupConstructor(
        isolate, exception_constructor, i::WASM_EXCEPTION_PACKAGE_TYPE,
        WasmExceptionPackage::kHeaderSize, "WebAssembly.Exception");
    InstallFunc(isolate, exception_proto, "getArg", WebAssemblyExceptionGetArg,
                2);
    InstallFunc(isolate, exception_proto, "is", WebAssemblyExceptionIs, 1);
    context->set_wasm_exception_constructor(*exception_constructor);
  }

  // Setup Suspender.
  if (enabled_features.has_stack_switching()) {
    Handle<JSFunction> suspender_constructor = InstallConstructorFunc(
        isolate, webassembly, "Suspender", WebAssemblySuspender);
    context->set_wasm_suspender_constructor(*suspender_constructor);
    SetupConstructor(isolate, suspender_constructor,
                     i::WASM_SUSPENDER_OBJECT_TYPE,
                     WasmSuspenderObject::kHeaderSize, "WebAssembly.Suspender");
  }

  // Setup Function
  if (enabled_features.has_type_reflection()) {
    Handle<JSFunction> function_constructor = InstallConstructorFunc(
        isolate, webassembly, "Function", WebAssemblyFunction);
    SetDummyInstanceTemplate(isolate, function_constructor);
    JSFunction::EnsureHasInitialMap(function_constructor);
    Handle<JSObject> function_proto(
        JSObject::cast(function_constructor->instance_prototype()), isolate);
    Handle<Map> function_map = isolate->factory()->CreateSloppyFunctionMap(
        FUNCTION_WITHOUT_PROTOTYPE, MaybeHandle<JSFunction>());
    CHECK(JSObject::SetPrototype(
              isolate, function_proto,
              handle(context->function_function().prototype(), isolate), false,
              kDontThrow)
              .FromJust());
    JSFunction::SetInitialMap(isolate, function_constructor, function_map,
                              function_proto);
    InstallFunc(isolate, function_constructor, "type", WebAssemblyFunctionType,
                1);
    // Make all exported functions an instance of {WebAssembly.Function}.
    context->set_wasm_exported_function_map(*function_map);
  } else {
    // Make all exported functions an instance of {Function}.
    Handle<Map> function_map = isolate->sloppy_function_without_prototype_map();
    context->set_wasm_exported_function_map(*function_map);
  }

  // Setup errors
  Handle<JSFunction> compile_error(
      isolate->native_context()->wasm_compile_error_function(), isolate);
  JSObject::AddProperty(isolate, webassembly,
                        isolate->factory()->CompileError_string(),
                        compile_error, DONT_ENUM);
  Handle<JSFunction> link_error(
      isolate->native_context()->wasm_link_error_function(), isolate);
  JSObject::AddProperty(isolate, webassembly,
                        isolate->factory()->LinkError_string(), link_error,
                        DONT_ENUM);
  Handle<JSFunction> runtime_error(
      isolate->native_context()->wasm_runtime_error_function(), isolate);
  JSObject::AddProperty(isolate, webassembly,
                        isolate->factory()->RuntimeError_string(),
                        runtime_error, DONT_ENUM);
}

// static
void WasmJs::InstallConditionalFeatures(Isolate* isolate,
                                        Handle<Context> context) {
  // Exception handling may have been enabled by an origin trial. If so, make
  // sure that the {WebAssembly.Tag} constructor is set up.
  auto enabled_features = i::wasm::WasmFeatures::FromContext(isolate, context);
  if (enabled_features.has_eh()) {
    Handle<JSGlobalObject> global = handle(context->global_object(), isolate);
    MaybeHandle<Object> maybe_webassembly =
        JSObject::GetProperty(isolate, global, "WebAssembly");
    Handle<Object> webassembly_obj;
    if (!maybe_webassembly.ToHandle(&webassembly_obj) ||
        !webassembly_obj->IsJSObject()) {
      // There is no {WebAssembly} object, or it's not what we expect.
      // Just return without adding the {Tag} constructor.
      return;
    }
    Handle<JSObject> webassembly = Handle<JSObject>::cast(webassembly_obj);
    // Setup Tag.
    Handle<String> tag_name = v8_str(isolate, "Tag");
    // The {WebAssembly} object may already have been modified. The following
    // code is designed to:
    //  - check for existing {Tag} properties on the object itself, and avoid
    //    overwriting them or adding duplicate properties
    //  - disregard any setters or read-only properties on the prototype chain
    //  - only make objects accessible to user code after all internal setup
    //    has been completed.
    if (JSObject::HasOwnProperty(isolate, webassembly, tag_name)
            .FromMaybe(true)) {
      // Existing property, or exception.
      return;
    }

    bool has_prototype = true;
    Handle<JSFunction> tag_constructor =
        CreateFunc(isolate, tag_name, WebAssemblyTag, has_prototype,
                   SideEffectType::kHasNoSideEffect);
    tag_constructor->shared().set_length(1);
    context->set_wasm_tag_constructor(*tag_constructor);
    Handle<JSObject> tag_proto =
        SetupConstructor(isolate, tag_constructor, i::WASM_TAG_OBJECT_TYPE,
                         WasmTagObject::kHeaderSize, "WebAssembly.Tag");
    if (enabled_features.has_type_reflection()) {
      InstallFunc(isolate, tag_proto, "type", WebAssemblyTagType, 0);
    }
    LookupIterator it(isolate, webassembly, tag_name, LookupIterator::OWN);
    Maybe<bool> result = JSObject::DefineOwnPropertyIgnoreAttributes(
        &it, tag_constructor, DONT_ENUM, Just(kDontThrow));
    // This could still fail if the object was non-extensible, but now we
    // return anyway so there's no need to even check.
    USE(result);
  }
}
#undef ASSIGN
#undef EXTRACT_THIS

}  // namespace internal
}  // namespace v8
