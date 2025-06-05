// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/compilation-cache-table.h"

#include "src/codegen/script-details.h"
#include "src/common/assert-scope.h"
#include "src/objects/compilation-cache-table-inl.h"

namespace v8 {
namespace internal {

namespace {

const int kLiteralEntryLength = 2;
const int kLiteralInitialLength = 2;
const int kLiteralContextOffset = 0;
const int kLiteralLiteralsOffset = 1;

int SearchLiteralsMapEntry(CompilationCacheTable cache,
                           InternalIndex cache_entry, Context native_context) {
  DisallowGarbageCollection no_gc;
  DCHECK(native_context.IsNativeContext());
  Object obj = cache.EvalFeedbackValueAt(cache_entry);

  // Check that there's no confusion between FixedArray and WeakFixedArray (the
  // object used to be a FixedArray here).
  DCHECK(!obj.IsFixedArray());
  if (obj.IsWeakFixedArray()) {
    WeakFixedArray literals_map = WeakFixedArray::cast(obj);
    int length = literals_map.length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      DCHECK(literals_map.Get(i + kLiteralContextOffset)->IsWeakOrCleared());
      if (literals_map.Get(i + kLiteralContextOffset) ==
          HeapObjectReference::Weak(native_context)) {
        return i;
      }
    }
  }
  return -1;
}

void AddToFeedbackCellsMap(Handle<CompilationCacheTable> cache,
                           InternalIndex cache_entry,
                           Handle<Context> native_context,
                           Handle<FeedbackCell> feedback_cell) {
  Isolate* isolate = native_context->GetIsolate();
  DCHECK(native_context->IsNativeContext());
  static_assert(kLiteralEntryLength == 2);
  Handle<WeakFixedArray> new_literals_map;
  int entry;

  Object obj = cache->EvalFeedbackValueAt(cache_entry);

  // Check that there's no confusion between FixedArray and WeakFixedArray (the
  // object used to be a FixedArray here).
  DCHECK(!obj.IsFixedArray());
  if (!obj.IsWeakFixedArray() || WeakFixedArray::cast(obj).length() == 0) {
    new_literals_map = isolate->factory()->NewWeakFixedArray(
        kLiteralInitialLength, AllocationType::kOld);
    entry = 0;
  } else {
    Handle<WeakFixedArray> old_literals_map(WeakFixedArray::cast(obj), isolate);
    entry = SearchLiteralsMapEntry(*cache, cache_entry, *native_context);
    if (entry >= 0) {
      // Just set the code of the entry.
      old_literals_map->Set(entry + kLiteralLiteralsOffset,
                            HeapObjectReference::Weak(*feedback_cell));
      return;
    }

    // Can we reuse an entry?
    DCHECK_LT(entry, 0);
    int length = old_literals_map->length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      if (old_literals_map->Get(i + kLiteralContextOffset)->IsCleared()) {
        new_literals_map = old_literals_map;
        entry = i;
        break;
      }
    }

    if (entry < 0) {
      // Copy old optimized code map and append one new entry.
      new_literals_map = isolate->factory()->CopyWeakFixedArrayAndGrow(
          old_literals_map, kLiteralEntryLength);
      entry = old_literals_map->length();
    }
  }

  new_literals_map->Set(entry + kLiteralContextOffset,
                        HeapObjectReference::Weak(*native_context));
  new_literals_map->Set(entry + kLiteralLiteralsOffset,
                        HeapObjectReference::Weak(*feedback_cell));

#ifdef DEBUG
  for (int i = 0; i < new_literals_map->length(); i += kLiteralEntryLength) {
    MaybeObject object = new_literals_map->Get(i + kLiteralContextOffset);
    DCHECK(object->IsCleared() ||
           object->GetHeapObjectAssumeWeak().IsNativeContext());
    object = new_literals_map->Get(i + kLiteralLiteralsOffset);
    DCHECK(object->IsCleared() ||
           object->GetHeapObjectAssumeWeak().IsFeedbackCell());
  }
#endif

  Object old_literals_map = cache->EvalFeedbackValueAt(cache_entry);
  if (old_literals_map != *new_literals_map) {
    cache->SetEvalFeedbackValueAt(cache_entry, *new_literals_map);
  }
}

FeedbackCell SearchLiteralsMap(CompilationCacheTable cache,
                               InternalIndex cache_entry,
                               Context native_context) {
  FeedbackCell result;
  int entry = SearchLiteralsMapEntry(cache, cache_entry, native_context);
  if (entry >= 0) {
    WeakFixedArray literals_map =
        WeakFixedArray::cast(cache.EvalFeedbackValueAt(cache_entry));
    DCHECK_LE(entry + kLiteralEntryLength, literals_map.length());
    MaybeObject object = literals_map.Get(entry + kLiteralLiteralsOffset);

    if (!object->IsCleared()) {
      result = FeedbackCell::cast(object->GetHeapObjectAssumeWeak());
    }
  }
  DCHECK(result.is_null() || result.IsFeedbackCell());
  return result;
}

// EvalCacheKeys are used as keys in the eval cache.
class EvalCacheKey : public HashTableKey {
 public:
  // This tuple unambiguously identifies calls to eval() or
  // CreateDynamicFunction() (such as through the Function() constructor).
  // * source is the string passed into eval(). For dynamic functions, this is
  //   the effective source for the function, some of which is implicitly
  //   generated.
  // * shared is the shared function info for the function containing the call
  //   to eval(). for dynamic functions, shared is the native context closure.
  // * When positive, position is the position in the source where eval is
  //   called. When negative, position is the negation of the position in the
  //   dynamic function's effective source where the ')' ends the parameters.
  EvalCacheKey(Handle<String> source, Handle<SharedFunctionInfo> shared,
               LanguageMode language_mode, int position)
      : HashTableKey(CompilationCacheShape::EvalHash(*source, *shared,
                                                     language_mode, position)),
        source_(source),
        shared_(shared),
        language_mode_(language_mode),
        position_(position) {}

  bool IsMatch(Object other) override {
    DisallowGarbageCollection no_gc;
    if (!other.IsFixedArray()) {
      DCHECK(other.IsNumber());
      uint32_t other_hash = static_cast<uint32_t>(other.Number());
      return Hash() == other_hash;
    }
    FixedArray other_array = FixedArray::cast(other);
    DCHECK(other_array.get(0).IsSharedFunctionInfo());
    if (*shared_ != other_array.get(0)) return false;
    int language_unchecked = Smi::ToInt(other_array.get(2));
    DCHECK(is_valid_language_mode(language_unchecked));
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    if (language_mode != language_mode_) return false;
    int position = Smi::ToInt(other_array.get(3));
    if (position != position_) return false;
    String source = String::cast(other_array.get(1));
    return source.Equals(*source_);
  }

  Handle<Object> AsHandle(Isolate* isolate) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(4);
    array->set(0, *shared_);
    array->set(1, *source_);
    array->set(2, Smi::FromEnum(language_mode_));
    array->set(3, Smi::FromInt(position_));
    array->set_map(ReadOnlyRoots(isolate).fixed_cow_array_map());
    return array;
  }

 private:
  Handle<String> source_;
  Handle<SharedFunctionInfo> shared_;
  LanguageMode language_mode_;
  int position_;
};

// RegExpKey carries the source and flags of a regular expression as key.
class RegExpKey : public HashTableKey {
 public:
  RegExpKey(Handle<String> string, JSRegExp::Flags flags)
      : HashTableKey(
            CompilationCacheShape::RegExpHash(*string, Smi::FromInt(flags))),
        string_(string),
        flags_(Smi::FromInt(flags)) {}

  // Rather than storing the key in the hash table, a pointer to the
  // stored value is stored where the key should be.  IsMatch then
  // compares the search key to the found object, rather than comparing
  // a key to a key.
  bool IsMatch(Object obj) override {
    FixedArray val = FixedArray::cast(obj);
    return string_->Equals(String::cast(val.get(JSRegExp::kSourceIndex))) &&
           (flags_ == val.get(JSRegExp::kFlagsIndex));
  }

  Handle<String> string_;
  Smi flags_;
};

// CodeKey carries the SharedFunctionInfo key associated with a Code
// object value.
class CodeKey : public HashTableKey {
 public:
  explicit CodeKey(Handle<SharedFunctionInfo> key)
      : HashTableKey(key->Hash()), key_(key) {}

  bool IsMatch(Object string) override { return *key_ == string; }

  Handle<SharedFunctionInfo> key_;
};

Smi ScriptHash(String source, MaybeHandle<Object> maybe_name, int line_offset,
               int column_offset, v8::ScriptOriginOptions origin_options,
               Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  size_t hash = base::hash_combine(source.EnsureHash());
  if (Handle<Object> name;
      maybe_name.ToHandle(&name) && name->IsString(isolate)) {
    hash =
        base::hash_combine(hash, String::cast(*name).EnsureHash(), line_offset,
                           column_offset, origin_options.Flags());
  }
  // The upper bits of the hash are discarded so that the value fits in a Smi.
  return Smi::From31BitPattern(static_cast<int>(hash & (~(1u << 31))));
}

}  // namespace

// We only re-use a cached function for some script source code if the
// script originates from the same place. This is to avoid issues
// when reporting errors, etc.
bool ScriptCacheKey::MatchesOrigin(Script script) {
  DisallowGarbageCollection no_gc;

  // If the script name isn't set, the boilerplate script should have
  // an undefined name to have the same origin.
  Handle<Object> name;
  if (!name_.ToHandle(&name)) {
    return script.name().IsUndefined(isolate_);
  }
  // Do the fast bailout checks first.
  if (line_offset_ != script.line_offset()) return false;
  if (column_offset_ != script.column_offset()) return false;
  // Check that both names are strings. If not, no match.
  if (!name->IsString(isolate_) || !script.name().IsString(isolate_))
    return false;
  // Are the origin_options same?
  if (origin_options_.Flags() != script.origin_options().Flags()) {
    return false;
  }
  // Compare the two name strings for equality.
  if (!String::cast(*name).Equals(String::cast(script.name()))) {
    return false;
  }

  // TODO(cbruni, chromium:1244145): Remove once migrated to the context
  Handle<Object> maybe_host_defined_options;
  if (!host_defined_options_.ToHandle(&maybe_host_defined_options)) {
    maybe_host_defined_options = isolate_->factory()->empty_fixed_array();
  }
  FixedArray host_defined_options =
      FixedArray::cast(*maybe_host_defined_options);
  FixedArray script_options = FixedArray::cast(script.host_defined_options());
  int length = host_defined_options.length();
  if (length != script_options.length()) return false;

  for (int i = 0; i < length; i++) {
    // host-defined options is a v8::PrimitiveArray.
    DCHECK(host_defined_options.get(i).IsPrimitive());
    DCHECK(script_options.get(i).IsPrimitive());
    if (!host_defined_options.get(i).StrictEquals(script_options.get(i))) {
      return false;
    }
  }
  return true;
}

ScriptCacheKey::ScriptCacheKey(Handle<String> source,
                               const ScriptDetails* script_details,
                               Isolate* isolate)
    : ScriptCacheKey(source, script_details->name_obj,
                     script_details->line_offset, script_details->column_offset,
                     script_details->origin_options,
                     script_details->host_defined_options, isolate) {}

ScriptCacheKey::ScriptCacheKey(Handle<String> source, MaybeHandle<Object> name,
                               int line_offset, int column_offset,
                               v8::ScriptOriginOptions origin_options,
                               MaybeHandle<Object> host_defined_options,
                               Isolate* isolate)
    : HashTableKey(static_cast<uint32_t>(ScriptHash(*source, name, line_offset,
                                                    column_offset,
                                                    origin_options, isolate)
                                             .value())),
      source_(source),
      name_(name),
      line_offset_(line_offset),
      column_offset_(column_offset),
      origin_options_(origin_options),
      host_defined_options_(host_defined_options),
      isolate_(isolate) {
  DCHECK(Smi::IsValid(static_cast<int>(Hash())));
}

bool ScriptCacheKey::IsMatch(Object other) {
  DisallowGarbageCollection no_gc;
  DCHECK(other.IsWeakFixedArray());
  WeakFixedArray other_array = WeakFixedArray::cast(other);
  DCHECK_EQ(other_array.length(), kEnd);

  // A hash check can quickly reject many non-matches, even though this step
  // isn't strictly necessary.
  uint32_t other_hash =
      static_cast<uint32_t>(other_array.Get(kHash).ToSmi().value());
  if (other_hash != Hash()) return false;

  HeapObject other_script_object;
  if (!other_array.Get(kWeakScript).GetHeapObjectIfWeak(&other_script_object)) {
    return false;
  }
  Script other_script = Script::cast(other_script_object);
  String other_source = String::cast(other_script.source());
  return other_source.Equals(*source_) && MatchesOrigin(other_script);
}

Handle<Object> ScriptCacheKey::AsHandle(Isolate* isolate,
                                        Handle<SharedFunctionInfo> shared) {
  Handle<WeakFixedArray> array = isolate->factory()->NewWeakFixedArray(kEnd);
  // Any SharedFunctionInfo being stored in the script cache should have a
  // Script.
  DCHECK(shared->script().IsScript());
  array->Set(kHash,
             MaybeObject::FromObject(Smi::FromInt(static_cast<int>(Hash()))));
  array->Set(kWeakScript,
             MaybeObject::MakeWeak(MaybeObject::FromObject(shared->script())));
  return array;
}

CompilationCacheScriptLookupResult::RawObjects
CompilationCacheScriptLookupResult::GetRawObjects() const {
  RawObjects result;
  if (Handle<Script> script; script_.ToHandle(&script)) {
    result.first = *script;
  }
  if (Handle<SharedFunctionInfo> toplevel_sfi;
      toplevel_sfi_.ToHandle(&toplevel_sfi)) {
    result.second = *toplevel_sfi;
  }
  return result;
}

CompilationCacheScriptLookupResult
CompilationCacheScriptLookupResult::FromRawObjects(
    CompilationCacheScriptLookupResult::RawObjects raw, Isolate* isolate) {
  CompilationCacheScriptLookupResult result;
  if (!raw.first.is_null()) {
    result.script_ = handle(raw.first, isolate);
  }
  if (!raw.second.is_null()) {
    result.is_compiled_scope_ = raw.second.is_compiled_scope(isolate);
    if (result.is_compiled_scope_.is_compiled()) {
      result.toplevel_sfi_ = handle(raw.second, isolate);
    }
  }
  return result;
}

CompilationCacheScriptLookupResult CompilationCacheTable::LookupScript(
    Handle<CompilationCacheTable> table, Handle<String> src,
    const ScriptDetails& script_details, Isolate* isolate) {
  src = String::Flatten(isolate, src);
  ScriptCacheKey key(src, &script_details, isolate);
  InternalIndex entry = table->FindEntry(isolate, &key);
  if (entry.is_not_found()) return {};

  DisallowGarbageCollection no_gc;
  Object key_in_table = table->KeyAt(entry);
  Script script = Script::cast(WeakFixedArray::cast(key_in_table)
                                   .Get(ScriptCacheKey::kWeakScript)
                                   .GetHeapObjectAssumeWeak());

  Object obj = table->PrimaryValueAt(entry);
  SharedFunctionInfo toplevel_sfi;
  if (!obj.IsUndefined(isolate)) {
    toplevel_sfi = SharedFunctionInfo::cast(obj);
    DCHECK_EQ(toplevel_sfi.script(), script);
  }

  return CompilationCacheScriptLookupResult::FromRawObjects(
      std::make_pair(script, toplevel_sfi), isolate);
}

InfoCellPair CompilationCacheTable::LookupEval(
    Handle<CompilationCacheTable> table, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<Context> native_context,
    LanguageMode language_mode, int position) {
  InfoCellPair empty_result;
  Isolate* isolate = native_context->GetIsolate();
  src = String::Flatten(isolate, src);

  EvalCacheKey key(src, outer_info, language_mode, position);
  InternalIndex entry = table->FindEntry(isolate, &key);
  if (entry.is_not_found()) return empty_result;

  if (!table->KeyAt(entry).IsFixedArray()) return empty_result;
  Object obj = table->PrimaryValueAt(entry);
  if (!obj.IsSharedFunctionInfo()) return empty_result;

  static_assert(CompilationCacheShape::kEntrySize == 3);
  FeedbackCell feedback_cell =
      SearchLiteralsMap(*table, entry, *native_context);
  return InfoCellPair(isolate, SharedFunctionInfo::cast(obj), feedback_cell);
}

Handle<Object> CompilationCacheTable::LookupRegExp(Handle<String> src,
                                                   JSRegExp::Flags flags) {
  Isolate* isolate = GetIsolate();
  DisallowGarbageCollection no_gc;
  RegExpKey key(src, flags);
  InternalIndex entry = FindEntry(isolate, &key);
  if (entry.is_not_found()) return isolate->factory()->undefined_value();
  return Handle<Object>(PrimaryValueAt(entry), isolate);
}

Handle<CompilationCacheTable> CompilationCacheTable::EnsureScriptTableCapacity(
    Isolate* isolate, Handle<CompilationCacheTable> cache) {
  if (cache->HasSufficientCapacityToAdd(1)) return cache;

  // Before resizing, delete are any entries whose keys contain cleared weak
  // pointers.
  {
    DisallowGarbageCollection no_gc;
    for (InternalIndex entry : cache->IterateEntries()) {
      Object key;
      if (!cache->ToKey(isolate, entry, &key)) continue;
      if (WeakFixedArray::cast(key)
              .Get(ScriptCacheKey::kWeakScript)
              .IsCleared()) {
        DCHECK(cache->PrimaryValueAt(entry).IsUndefined());
        cache->RemoveEntry(entry);
      }
    }
  }

  return EnsureCapacity(isolate, cache);
}

Handle<CompilationCacheTable> CompilationCacheTable::PutScript(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<SharedFunctionInfo> value, Isolate* isolate) {
  src = String::Flatten(isolate, src);
  Handle<Script> script = handle(Script::cast(value->script()), isolate);
  MaybeHandle<Object> script_name;
  if (script->name().IsString(isolate)) {
    script_name = handle(script->name(), isolate);
  }
  Handle<FixedArray> host_defined_options(script->host_defined_options(),
                                          isolate);
  ScriptCacheKey key(src, script_name, script->line_offset(),
                     script->column_offset(), script->origin_options(),
                     host_defined_options, isolate);
  Handle<Object> k = key.AsHandle(isolate, value);

  // Check whether there is already a matching entry. If so, we must overwrite
  // it. This allows an entry whose value is undefined to upgrade to contain a
  // SharedFunctionInfo.
  InternalIndex entry = cache->FindEntry(isolate, &key);
  bool found_existing = entry.is_found();
  if (!found_existing) {
    cache = EnsureScriptTableCapacity(isolate, cache);
    entry = cache->FindInsertionEntry(isolate, key.Hash());
  }
  // We might be tempted to DCHECK here that the Script in the existing entry
  // matches the Script in the new key. However, replacing an existing Script
  // can still happen in some edge cases that aren't common enough to be worth
  // fixing. Consider the following unlikely sequence of events:
  // 1. BackgroundMergeTask::SetUpOnMainThread finds a script S1 in the cache.
  // 2. DevTools is attached and clears the cache.
  // 3. DevTools is detached; the cache is reenabled.
  // 4. A new instance of the script, S2, is compiled and placed into the cache.
  // 5. The merge from step 1 finishes on the main thread, still using S1, and
  //    places S1 into the cache, replacing S2.
  cache->SetKeyAt(entry, *k);
  cache->SetPrimaryValueAt(entry, *value);
  if (!found_existing) {
    cache->ElementAdded();
  }
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutEval(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<SharedFunctionInfo> value,
    Handle<Context> native_context, Handle<FeedbackCell> feedback_cell,
    int position) {
  Isolate* isolate = native_context->GetIsolate();
  src = String::Flatten(isolate, src);
  EvalCacheKey key(src, outer_info, value->language_mode(), position);

  // This block handles 'real' insertions, i.e. the initial dummy insert
  // (below) has already happened earlier.
  {
    Handle<Object> k = key.AsHandle(isolate);
    InternalIndex entry = cache->FindEntry(isolate, &key);
    if (entry.is_found()) {
      cache->SetKeyAt(entry, *k);
      cache->SetPrimaryValueAt(entry, *value);
      // AddToFeedbackCellsMap may allocate a new sub-array to live in the
      // entry, but it won't change the cache array. Therefore EntryToIndex
      // and entry remains correct.
      AddToFeedbackCellsMap(cache, entry, native_context, feedback_cell);
      // Add hash again even on cache hit to avoid unnecessary cache delay in
      // case of hash collisions.
    }
  }

  // Create a dummy entry to mark that this key has already been inserted once.
  cache = EnsureCapacity(isolate, cache);
  InternalIndex entry = cache->FindInsertionEntry(isolate, key.Hash());
  Handle<Object> k =
      isolate->factory()->NewNumber(static_cast<double>(key.Hash()));
  cache->SetKeyAt(entry, *k);
  cache->SetPrimaryValueAt(entry, Smi::FromInt(kHashGenerations));
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutRegExp(
    Isolate* isolate, Handle<CompilationCacheTable> cache, Handle<String> src,
    JSRegExp::Flags flags, Handle<FixedArray> value) {
  RegExpKey key(src, flags);
  cache = EnsureCapacity(isolate, cache);
  InternalIndex entry = cache->FindInsertionEntry(isolate, key.Hash());
  // We store the value in the key slot, and compare the search key
  // to the stored value with a custom IsMatch function during lookups.
  cache->SetKeyAt(entry, *value);
  cache->SetPrimaryValueAt(entry, *value);
  cache->ElementAdded();
  return cache;
}

void CompilationCacheTable::Remove(Object value) {
  DisallowGarbageCollection no_gc;
  for (InternalIndex entry : IterateEntries()) {
    if (PrimaryValueAt(entry) == value) {
      RemoveEntry(entry);
    }
  }
}

void CompilationCacheTable::RemoveEntry(InternalIndex entry) {
  int entry_index = EntryToIndex(entry);
  Object the_hole_value = GetReadOnlyRoots().the_hole_value();
  for (int i = 0; i < kEntrySize; i++) {
    NoWriteBarrierSet(*this, entry_index + i, the_hole_value);
  }
  ElementRemoved();

  // This table does not shrink upon deletion. The script cache depends on that
  // fact, because EnsureScriptTableCapacity calls RemoveEntry at a time when
  // shrinking the table would be counterproductive.
}

}  // namespace internal
}  // namespace v8
