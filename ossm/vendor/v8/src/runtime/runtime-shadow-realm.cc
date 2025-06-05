// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arguments-inl.h"
#include "src/objects/js-function.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_ShadowRealmWrappedFunctionCreate) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = args.at<NativeContext>(0);
  Handle<JSReceiver> value = args.at<JSReceiver>(1);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSWrappedFunction::Create(isolate, native_context, value));
}

// https://tc39.es/proposal-shadowrealm/#sec-shadowrealm.prototype.importvalue
RUNTIME_FUNCTION(Runtime_ShadowRealmImportValue) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  Handle<String> specifier = args.at<String>(0);

  Handle<JSPromise> inner_capability;

  MaybeHandle<Object> import_assertions;
  MaybeHandle<Script> referrer;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, inner_capability,
      isolate->RunHostImportModuleDynamicallyCallback(referrer, specifier,
                                                      import_assertions));
  // Check that the promise is created in the eval_context.
  DCHECK(
      inner_capability->GetCreationContext().ToHandleChecked().is_identical_to(
          isolate->native_context()));

  return *inner_capability;
}

}  // namespace internal
}  // namespace v8
