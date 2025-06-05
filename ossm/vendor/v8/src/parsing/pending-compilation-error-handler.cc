// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/pending-compilation-error-handler.h"

#include "src/ast/ast-value-factory.h"
#include "src/base/export-template.h"
#include "src/base/logging.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/execution/messages.h"
#include "src/handles/handles.h"
#include "src/heap/local-heap-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

void PendingCompilationErrorHandler::MessageDetails::SetString(
    Handle<String> string, Isolate* isolate) {
  DCHECK_NE(args_[0].type, kMainThreadHandle);
  args_[0].type = kMainThreadHandle;
  args_[0].js_string = string;
}

void PendingCompilationErrorHandler::MessageDetails::SetString(
    Handle<String> string, LocalIsolate* isolate) {
  DCHECK_NE(args_[0].type, kMainThreadHandle);
  args_[0].type = kMainThreadHandle;
  args_[0].js_string = isolate->heap()->NewPersistentHandle(string);
}

template <typename IsolateT>
void PendingCompilationErrorHandler::MessageDetails::Prepare(
    IsolateT* isolate) {
  for (int i = 0; i < kMaxArgumentCount; i++) {
    switch (args_[i].type) {
      case kAstRawString:
        return SetString(args_[i].ast_string->string(), isolate);

      case kNone:
      case kConstCharString:
        // We can delay allocation until ArgString(isolate).
        return;

      case kMainThreadHandle:
        // The message details might already be prepared, so skip them if this
        // is the case.
        return;
    }
  }
}

Handle<String> PendingCompilationErrorHandler::MessageDetails::ArgString(
    Isolate* isolate, int index) const {
  // `index` may be >= argc; in that case we return a default value to pass on
  // elsewhere.
  DCHECK_LT(index, kMaxArgumentCount);
  switch (args_[index].type) {
    case kMainThreadHandle:
      return args_[index].js_string;
    case kNone:
      return Handle<String>::null();
    case kConstCharString:
      return isolate->factory()
          ->NewStringFromUtf8(base::CStrVector(args_[index].c_string),
                              AllocationType::kOld)
          .ToHandleChecked();
    case kAstRawString:
      UNREACHABLE();
  }
}

MessageLocation PendingCompilationErrorHandler::MessageDetails::GetLocation(
    Handle<Script> script) const {
  return MessageLocation(script, start_position_, end_position_);
}

void PendingCompilationErrorHandler::ReportMessageAt(int start_position,
                                                     int end_position,
                                                     MessageTemplate message,
                                                     const char* arg) {
  if (has_pending_error_) return;
  has_pending_error_ = true;

  error_details_ = MessageDetails(start_position, end_position, message, arg);
}

void PendingCompilationErrorHandler::ReportMessageAt(int start_position,
                                                     int end_position,
                                                     MessageTemplate message,
                                                     const AstRawString* arg) {
  if (has_pending_error_) return;
  has_pending_error_ = true;

  error_details_ = MessageDetails(start_position, end_position, message, arg);
}

void PendingCompilationErrorHandler::ReportMessageAt(int start_position,
                                                     int end_position,
                                                     MessageTemplate message,
                                                     const AstRawString* arg0,
                                                     const char* arg1) {
  if (has_pending_error_) return;
  has_pending_error_ = true;
  error_details_ =
      MessageDetails(start_position, end_position, message, arg0, arg1);
}

void PendingCompilationErrorHandler::ReportWarningAt(int start_position,
                                                     int end_position,
                                                     MessageTemplate message,
                                                     const char* arg) {
  warning_messages_.emplace_front(
      MessageDetails(start_position, end_position, message, arg));
}

template <typename IsolateT>
void PendingCompilationErrorHandler::PrepareWarnings(IsolateT* isolate) {
  DCHECK(!has_pending_error());

  for (MessageDetails& warning : warning_messages_) {
    warning.Prepare(isolate);
  }
}
template void PendingCompilationErrorHandler::PrepareWarnings(Isolate* isolate);
template void PendingCompilationErrorHandler::PrepareWarnings(
    LocalIsolate* isolate);

void PendingCompilationErrorHandler::ReportWarnings(
    Isolate* isolate, Handle<Script> script) const {
  DCHECK(!has_pending_error());

  for (const MessageDetails& warning : warning_messages_) {
    MessageLocation location = warning.GetLocation(script);
    Handle<String> argument = warning.ArgString(isolate, 0);
    DCHECK_LT(warning.ArgCount(), 2);  // Arg1 is only used for errors.
    Handle<JSMessageObject> message =
        MessageHandler::MakeMessageObject(isolate, warning.message(), &location,
                                          argument, Handle<FixedArray>::null());
    message->set_error_level(v8::Isolate::kMessageWarning);
    MessageHandler::ReportMessage(isolate, &location, message);
  }
}

template <typename IsolateT>
void PendingCompilationErrorHandler::PrepareErrors(
    IsolateT* isolate, AstValueFactory* ast_value_factory) {
  if (stack_overflow()) return;

  DCHECK(has_pending_error());
  // Internalize ast values for throwing the pending error.
  ast_value_factory->Internalize(isolate);
  error_details_.Prepare(isolate);
}
template EXPORT_TEMPLATE_DEFINE(
    V8_EXPORT_PRIVATE) void PendingCompilationErrorHandler::
    PrepareErrors(Isolate* isolate, AstValueFactory* ast_value_factory);
template EXPORT_TEMPLATE_DEFINE(
    V8_EXPORT_PRIVATE) void PendingCompilationErrorHandler::
    PrepareErrors(LocalIsolate* isolate, AstValueFactory* ast_value_factory);

void PendingCompilationErrorHandler::ReportErrors(Isolate* isolate,
                                                  Handle<Script> script) const {
  if (stack_overflow()) {
    isolate->StackOverflow();
  } else {
    DCHECK(has_pending_error());
    ThrowPendingError(isolate, script);
  }
}

void PendingCompilationErrorHandler::ThrowPendingError(
    Isolate* isolate, Handle<Script> script) const {
  if (!has_pending_error_) return;

  MessageLocation location = error_details_.GetLocation(script);
  Handle<String> arg0 = error_details_.ArgString(isolate, 0);
  Handle<String> arg1 = error_details_.ArgString(isolate, 1);
  isolate->debug()->OnCompileError(script);

  Factory* factory = isolate->factory();
  Handle<JSObject> error =
      factory->NewSyntaxError(error_details_.message(), arg0, arg1);
  isolate->ThrowAt(error, &location);
}

Handle<String> PendingCompilationErrorHandler::FormatErrorMessageForTest(
    Isolate* isolate) {
  error_details_.Prepare(isolate);
  return MessageFormatter::Format(isolate, error_details_.message(),
                                  error_details_.ArgString(isolate, 0),
                                  error_details_.ArgString(isolate, 1));
}

}  // namespace internal
}  // namespace v8
