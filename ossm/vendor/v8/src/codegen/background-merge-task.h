// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_BACKGROUND_MERGE_TASK_H_
#define V8_CODEGEN_BACKGROUND_MERGE_TASK_H_

#include <vector>

#include "src/handles/maybe-handles.h"

namespace v8 {
namespace internal {

class FeedbackMetadata;
class PersistentHandles;
class Script;
class SharedFunctionInfo;
class String;

struct ScriptDetails;

// Contains data transferred between threads for background merging between a
// newly compiled or deserialized script and an existing script from the Isolate
// compilation cache.
class V8_EXPORT_PRIVATE BackgroundMergeTask {
 public:
  ~BackgroundMergeTask();

  // Step 1: on the main thread, check whether the Isolate compilation cache
  // contains the script.
  void SetUpOnMainThread(Isolate* isolate, Handle<String> source_text,
                         const ScriptDetails& script_details,
                         LanguageMode language_mode);

  // Step 2: on the background thread, update pointers in the new Script's
  // object graph to point to corresponding objects from the cached Script where
  // appropriate. May only be called if HasCachedScript returned true.
  void BeginMergeInBackground(LocalIsolate* isolate, Handle<Script> new_script);

  // Step 3: on the main thread again, complete the merge so that all relevant
  // objects are reachable from the cached Script. May only be called if
  // HasPendingForegroundWork returned true. Returns the top-level
  // SharedFunctionInfo that should be used.
  Handle<SharedFunctionInfo> CompleteMergeInForeground(
      Isolate* isolate, Handle<Script> new_script);

  bool HasCachedScript() const { return !cached_script_.is_null(); }
  bool HasPendingForegroundWork() const {
    return !used_new_sfis_.empty() ||
           !new_compiled_data_for_cached_sfis_.empty();
  }

 private:
  std::unique_ptr<PersistentHandles> persistent_handles_;

  // Data from main thread:

  MaybeHandle<Script> cached_script_;

  // Data from background thread:

  // The top-level SharedFunctionInfo from the cached script, if one existed,
  // just to keep it alive.
  MaybeHandle<SharedFunctionInfo> toplevel_sfi_from_cached_script_;

  // New SharedFunctionInfos which are used because there was no corresponding
  // SharedFunctionInfo in the cached script. The main thread must:
  // 1. Check whether the cached script gained corresponding SharedFunctionInfos
  //    for any of these, and if so, redo the merge.
  // 2. Update the cached script's shared_function_infos list to refer to these.
  std::vector<Handle<SharedFunctionInfo>> used_new_sfis_;

  // SharedFunctionInfos from the cached script which were not compiled, with
  // function_data and feedback_metadata from the corresponding new
  // SharedFunctionInfo. If the SharedFunctionInfo from the cached script is
  // still uncompiled when finishing, the main thread must set the two fields.
  struct NewCompiledDataForCachedSfi {
    Handle<SharedFunctionInfo> cached_sfi;
    Handle<Object> function_data;
    Handle<FeedbackMetadata> feedback_metadata;
  };
  std::vector<NewCompiledDataForCachedSfi> new_compiled_data_for_cached_sfis_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_BACKGROUND_MERGE_TASK_H_
