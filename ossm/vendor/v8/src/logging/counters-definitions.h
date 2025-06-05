// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_COUNTERS_DEFINITIONS_H_
#define V8_LOGGING_COUNTERS_DEFINITIONS_H_

#include "include/v8-internal.h"

namespace v8 {
namespace internal {

#define HISTOGRAM_RANGE_LIST(HR)                                               \
  /* Generic range histograms: HR(name, caption, min, max, num_buckets) */     \
  HR(background_marking, V8.GCBackgroundMarking, 0, 10000, 101)                \
  HR(background_sweeping, V8.GCBackgroundSweeping, 0, 10000, 101)              \
  HR(code_cache_reject_reason, V8.CodeCacheRejectReason, 1, 6, 6)              \
  HR(errors_thrown_per_context, V8.ErrorsThrownPerContext, 0, 200, 20)         \
  HR(debug_feature_usage, V8.DebugFeatureUsage, 1, 7, 7)                       \
  HR(incremental_marking_reason, V8.GCIncrementalMarkingReason, 0,             \
     kGarbageCollectionReasonMaxValue, kGarbageCollectionReasonMaxValue + 1)   \
  HR(incremental_marking_sum, V8.GCIncrementalMarkingSum, 0, 10000, 101)       \
  HR(mark_compact_reason, V8.GCMarkCompactReason, 0,                           \
     kGarbageCollectionReasonMaxValue, kGarbageCollectionReasonMaxValue + 1)   \
  HR(gc_finalize_clear, V8.GCFinalizeMC.Clear, 0, 10000, 101)                  \
  HR(gc_finalize_epilogue, V8.GCFinalizeMC.Epilogue, 0, 10000, 101)            \
  HR(gc_finalize_evacuate, V8.GCFinalizeMC.Evacuate, 0, 10000, 101)            \
  HR(gc_finalize_finish, V8.GCFinalizeMC.Finish, 0, 10000, 101)                \
  HR(gc_finalize_mark, V8.GCFinalizeMC.Mark, 0, 10000, 101)                    \
  HR(gc_finalize_prologue, V8.GCFinalizeMC.Prologue, 0, 10000, 101)            \
  HR(gc_finalize_sweep, V8.GCFinalizeMC.Sweep, 0, 10000, 101)                  \
  HR(gc_scavenger_scavenge_main, V8.GCScavenger.ScavengeMain, 0, 10000, 101)   \
  HR(gc_scavenger_scavenge_roots, V8.GCScavenger.ScavengeRoots, 0, 10000, 101) \
  HR(gc_marking_sum, V8.GCMarkingSum, 0, 10000, 101)                           \
  /* Range and bucket matches BlinkGC.MainThreadMarkingThroughput. */          \
  HR(gc_main_thread_marking_throughput, V8.GCMainThreadMarkingThroughput, 0,   \
     100000, 50)                                                               \
  HR(young_generation_handling, V8.GCYoungGenerationHandling, 0, 2, 3)         \
  /* Asm/Wasm. */                                                              \
  HR(wasm_functions_per_asm_module, V8.WasmFunctionsPerModule.asm, 1, 1000000, \
     51)                                                                       \
  HR(wasm_functions_per_wasm_module, V8.WasmFunctionsPerModule.wasm, 1,        \
     1000000, 51)                                                              \
  HR(array_buffer_big_allocations, V8.ArrayBufferLargeAllocations, 0, 4096,    \
     13)                                                                       \
  HR(array_buffer_new_size_failures, V8.ArrayBufferNewSizeFailures, 0, 4096,   \
     13)                                                                       \
  HR(shared_array_allocations, V8.SharedArrayAllocationSizes, 0, 4096, 13)     \
  HR(wasm_asm_huge_function_size_bytes, V8.WasmHugeFunctionSizeBytes.asm,      \
     100 * KB, GB, 51)                                                         \
  HR(wasm_wasm_huge_function_size_bytes, V8.WasmHugeFunctionSizeBytes.wasm,    \
     100 * KB, GB, 51)                                                         \
  HR(wasm_asm_module_size_bytes, V8.WasmModuleSizeBytes.asm, 1, GB, 51)        \
  HR(wasm_wasm_module_size_bytes, V8.WasmModuleSizeBytes.wasm, 1, GB, 51)      \
  HR(wasm_asm_min_mem_pages_count, V8.WasmMinMemPagesCount.asm, 1, 2 << 16,    \
     51)                                                                       \
  HR(wasm_wasm_min_mem_pages_count, V8.WasmMinMemPagesCount.wasm, 1, 2 << 16,  \
     51)                                                                       \
  HR(wasm_wasm_max_mem_pages_count, V8.WasmMaxMemPagesCount.wasm, 1, 2 << 16,  \
     51)                                                                       \
  HR(wasm_compile_function_peak_memory_bytes,                                  \
     V8.WasmCompileFunctionPeakMemoryBytes, 1, GB, 51)                         \
  HR(wasm_compile_huge_function_peak_memory_bytes,                             \
     V8.WasmCompileHugeFunctionPeakMemoryBytes, 1, GB, 51)                     \
  HR(asm_module_size_bytes, V8.AsmModuleSizeBytes, 1, GB, 51)                  \
  HR(compile_script_cache_behaviour, V8.CompileScript.CacheBehaviour, 0, 20,   \
     21)                                                                       \
  HR(wasm_memory_allocation_result, V8.WasmMemoryAllocationResult, 0, 3, 4)    \
  /* committed code size per module, collected on GC */                        \
  HR(wasm_module_code_size_mb, V8.WasmModuleCodeSizeMiB, 0, 1024, 64)          \
  /* code size per module after baseline compilation */                        \
  HR(wasm_module_code_size_mb_after_baseline,                                  \
     V8.WasmModuleCodeSizeBaselineMiB, 0, 1024, 64)                            \
  /* percent of freed code size per module, collected on GC */                 \
  HR(wasm_module_freed_code_size_percent, V8.WasmModuleCodeSizePercentFreed,   \
     0, 100, 32)                                                               \
  /* number of code GCs triggered per native module, collected on code GC */   \
  HR(wasm_module_num_triggered_code_gcs,                                       \
     V8.WasmModuleNumberOfCodeGCsTriggered, 1, 128, 20)                        \
  /* number of code spaces reserved per wasm module */                         \
  HR(wasm_module_num_code_spaces, V8.WasmModuleNumberOfCodeSpaces, 1, 128, 20) \
  /* number of live modules per isolate */                                     \
  HR(wasm_modules_per_isolate, V8.WasmModulesPerIsolate, 1, 1024, 30)          \
  /* number of live modules per engine (i.e. whole process) */                 \
  HR(wasm_modules_per_engine, V8.WasmModulesPerEngine, 1, 1024, 30)            \
  /* bailout reason if Liftoff failed, or {kSuccess} (per function) */         \
  HR(liftoff_bailout_reasons, V8.LiftoffBailoutReasons, 0, 20, 21)             \
  /* support for PKEYs/PKU by testing result of pkey_alloc() */                \
  HR(wasm_memory_protection_keys_support, V8.WasmMemoryProtectionKeysSupport,  \
     0, 1, 2)                                                                  \
  /* number of thrown exceptions per isolate */                                \
  HR(wasm_throw_count, V8.WasmThrowCount, 0, 100000, 30)                       \
  /* number of rethrown exceptions per isolate */                              \
  HR(wasm_rethrow_count, V8.WasmReThrowCount, 0, 100000, 30)                   \
  /* number of caught exceptions per isolate */                                \
  HR(wasm_catch_count, V8.WasmCatchCount, 0, 100000, 30)                       \
  /* Ticks observed in a single Turbofan compilation, in 1K */                 \
  HR(turbofan_ticks, V8.TurboFan1KTicks, 0, 100000, 200)                       \
  /* Backtracks observed in a single regexp interpreter execution */           \
  /* The maximum of 100M backtracks takes roughly 2 seconds on my machine. */  \
  HR(regexp_backtracks, V8.RegExpBacktracks, 1, 100000000, 50)                 \
  /* number of times a cache event is triggered for a wasm module */           \
  HR(wasm_cache_count, V8.WasmCacheCount, 0, 100, 101)                         \
  HR(wasm_streaming_until_compilation_finished,                                \
     V8.WasmStreamingUntilCompilationFinishedMilliSeconds, 0, 10000, 50)       \
  HR(wasm_compilation_until_streaming_finished,                                \
     V8.WasmCompilationUntilStreamFinishedMilliSeconds, 0, 10000, 50)          \
  /* Number of in-use external pointers in the external pointer table */       \
  /* Counted after sweeping the table at the end of mark-compact GC */         \
  HR(external_pointers_count, V8.SandboxedExternalPointersCount, 0,            \
     kMaxExternalPointers, 101)                                                \
  HR(wasm_num_lazy_compilations_5sec, V8.WasmNumLazyCompilations5Sec, 0,       \
     200000, 50)                                                               \
  HR(wasm_num_lazy_compilations_20sec, V8.WasmNumLazyCompilations20Sec, 0,     \
     200000, 50)                                                               \
  HR(wasm_num_lazy_compilations_60sec, V8.WasmNumLazyCompilations60Sec, 0,     \
     200000, 50)                                                               \
  HR(wasm_num_lazy_compilations_120sec, V8.WasmNumLazyCompilations120Sec, 0,   \
     200000, 50)                                                               \
  /* Outcome of external pointer table compaction: kSuccess, */                \
  /* kPartialSuccessor kAbortedDuringSweeping. See */                          \
  /* ExternalPointerTable::TableCompactionOutcome enum for more details */     \
  HR(external_pointer_table_compaction_outcome,                                \
     V8.ExternalPointerTableCompactionOutcome, 0, 2, 3)

#define NESTED_TIMED_HISTOGRAM_LIST(HT)                                       \
  /* Nested timer histograms allow distributions of nested timed results. */  \
  /* HT(name, caption, max, unit) */                                          \
  /* Garbage collection timers. */                                            \
  HT(gc_idle_notification, V8.GCIdleNotification, 10000, MILLISECOND)         \
  HT(gc_incremental_marking, V8.GCIncrementalMarking, 10000, MILLISECOND)     \
  HT(gc_incremental_marking_start, V8.GCIncrementalMarkingStart, 10000,       \
     MILLISECOND)                                                             \
  HT(gc_incremental_marking_finalize, V8.GCIncrementalMarkingFinalize, 10000, \
     MILLISECOND)                                                             \
  HT(gc_low_memory_notification, V8.GCLowMemoryNotification, 10000,           \
     MILLISECOND)                                                             \
  /* Compilation times. */                                                    \
  HT(collect_source_positions, V8.CollectSourcePositions, 1000000,            \
     MICROSECOND)                                                             \
  HT(compile, V8.CompileMicroSeconds, 1000000, MICROSECOND)                   \
  HT(compile_eval, V8.CompileEvalMicroSeconds, 1000000, MICROSECOND)          \
  /* Serialization as part of compilation (code caching) */                   \
  HT(compile_serialize, V8.CompileSerializeMicroSeconds, 100000, MICROSECOND) \
  HT(compile_deserialize, V8.CompileDeserializeMicroSeconds, 1000000,         \
     MICROSECOND)                                                             \
  /* Total compilation time incl. caching/parsing */                          \
  HT(compile_script, V8.CompileScriptMicroSeconds, 1000000, MICROSECOND)

#define NESTED_TIMED_HISTOGRAM_LIST_SLOW(HT)                               \
  /* Total V8 time (including JS and runtime calls, exluding callbacks) */ \
  HT(execute, V8.ExecuteMicroSeconds, 1000000, MICROSECOND)

#define TIMED_HISTOGRAM_LIST(HT)                                               \
  /* Timer histograms, thread safe: HT(name, caption, max, unit) */            \
  /* Garbage collection timers. */                                             \
  HT(gc_compactor, V8.GCCompactor, 10000, MILLISECOND)                         \
  HT(gc_compactor_background, V8.GCCompactorBackground, 10000, MILLISECOND)    \
  HT(gc_compactor_foreground, V8.GCCompactorForeground, 10000, MILLISECOND)    \
  HT(gc_finalize, V8.GCFinalizeMC, 10000, MILLISECOND)                         \
  HT(gc_finalize_background, V8.GCFinalizeMCBackground, 10000, MILLISECOND)    \
  HT(gc_finalize_foreground, V8.GCFinalizeMCForeground, 10000, MILLISECOND)    \
  HT(gc_finalize_measure_memory, V8.GCFinalizeMCMeasureMemory, 10000,          \
     MILLISECOND)                                                              \
  HT(gc_finalize_reduce_memory, V8.GCFinalizeMCReduceMemory, 10000,            \
     MILLISECOND)                                                              \
  HT(gc_finalize_reduce_memory_background,                                     \
     V8.GCFinalizeMCReduceMemoryBackground, 10000, MILLISECOND)                \
  HT(gc_finalize_reduce_memory_foreground,                                     \
     V8.GCFinalizeMCReduceMemoryForeground, 10000, MILLISECOND)                \
  HT(measure_memory_delay_ms, V8.MeasureMemoryDelayMilliseconds, 100000,       \
     MILLISECOND)                                                              \
  HT(gc_time_to_global_safepoint, V8.GC.TimeToGlobalSafepoint, 10000000,       \
     MICROSECOND)                                                              \
  HT(gc_time_to_safepoint, V8.GC.TimeToSafepoint, 10000000, MICROSECOND)       \
  HT(gc_time_to_collection_on_background, V8.GC.TimeToCollectionOnBackground,  \
     10000000, MICROSECOND)                                                    \
  /* TurboFan timers. */                                                       \
  HT(turbofan_optimize_prepare, V8.TurboFanOptimizePrepare, 1000000,           \
     MICROSECOND)                                                              \
  HT(turbofan_optimize_execute, V8.TurboFanOptimizeExecute, 1000000,           \
     MICROSECOND)                                                              \
  HT(turbofan_optimize_finalize, V8.TurboFanOptimizeFinalize, 1000000,         \
     MICROSECOND)                                                              \
  HT(turbofan_optimize_total_foreground, V8.TurboFanOptimizeTotalForeground,   \
     10000000, MICROSECOND)                                                    \
  HT(turbofan_optimize_total_background, V8.TurboFanOptimizeTotalBackground,   \
     10000000, MICROSECOND)                                                    \
  HT(turbofan_optimize_total_time, V8.TurboFanOptimizeTotalTime, 10000000,     \
     MICROSECOND)                                                              \
  HT(turbofan_optimize_non_concurrent_total_time,                              \
     V8.TurboFanOptimizeNonConcurrentTotalTime, 10000000, MICROSECOND)         \
  HT(turbofan_optimize_concurrent_total_time,                                  \
     V8.TurboFanOptimizeConcurrentTotalTime, 10000000, MICROSECOND)            \
  HT(turbofan_osr_prepare, V8.TurboFanOptimizeForOnStackReplacementPrepare,    \
     1000000, MICROSECOND)                                                     \
  HT(turbofan_osr_execute, V8.TurboFanOptimizeForOnStackReplacementExecute,    \
     1000000, MICROSECOND)                                                     \
  HT(turbofan_osr_finalize, V8.TurboFanOptimizeForOnStackReplacementFinalize,  \
     1000000, MICROSECOND)                                                     \
  HT(turbofan_osr_total_time,                                                  \
     V8.TurboFanOptimizeForOnStackReplacementTotalTime, 10000000, MICROSECOND) \
  /* Wasm timers. */                                                           \
  HT(wasm_compile_asm_module_time, V8.WasmCompileModuleMicroSeconds.asm,       \
     10000000, MICROSECOND)                                                    \
  HT(wasm_compile_wasm_module_time, V8.WasmCompileModuleMicroSeconds.wasm,     \
     10000000, MICROSECOND)                                                    \
  HT(wasm_async_compile_wasm_module_time,                                      \
     V8.WasmCompileModuleAsyncMicroSeconds, 100000000, MICROSECOND)            \
  HT(wasm_streaming_compile_wasm_module_time,                                  \
     V8.WasmCompileModuleStreamingMicroSeconds, 100000000, MICROSECOND)        \
  HT(wasm_streaming_finish_wasm_module_time,                                   \
     V8.WasmFinishModuleStreamingMicroSeconds, 100000000, MICROSECOND)         \
  HT(wasm_deserialization_time, V8.WasmDeserializationTimeMilliSeconds, 10000, \
     MILLISECOND)                                                              \
  HT(wasm_tier_up_module_time, V8.WasmTierUpModuleMicroSeconds, 100000000,     \
     MICROSECOND)                                                              \
  HT(wasm_compile_asm_function_time, V8.WasmCompileFunctionMicroSeconds.asm,   \
     1000000, MICROSECOND)                                                     \
  HT(wasm_compile_wasm_function_time, V8.WasmCompileFunctionMicroSeconds.wasm, \
     1000000, MICROSECOND)                                                     \
  HT(wasm_compile_huge_function_time, V8.WasmCompileHugeFunctionMilliSeconds,  \
     100000, MILLISECOND)                                                      \
  HT(wasm_instantiate_wasm_module_time,                                        \
     V8.WasmInstantiateModuleMicroSeconds.wasm, 10000000, MICROSECOND)         \
  HT(wasm_instantiate_asm_module_time,                                         \
     V8.WasmInstantiateModuleMicroSeconds.asm, 10000000, MICROSECOND)          \
  HT(wasm_time_between_throws, V8.WasmTimeBetweenThrowsMilliseconds, 1000,     \
     MILLISECOND)                                                              \
  HT(wasm_time_between_rethrows, V8.WasmTimeBetweenRethrowsMilliseconds, 1000, \
     MILLISECOND)                                                              \
  HT(wasm_time_between_catch, V8.WasmTimeBetweenCatchMilliseconds, 1000,       \
     MILLISECOND)                                                              \
  HT(wasm_lazy_compile_time, V8.WasmLazyCompileTimeMicroSeconds, 100000000,    \
     MICROSECOND)                                                              \
  HT(wasm_compile_after_deserialize,                                           \
     V8.WasmCompileAfterDeserializeMilliSeconds, 1000000, MILLISECOND)         \
  /* Total compilation time incl. caching/parsing for various cache states. */ \
  HT(compile_script_with_produce_cache,                                        \
     V8.CompileScriptMicroSeconds.ProduceCache, 1000000, MICROSECOND)          \
  HT(compile_script_with_isolate_cache_hit,                                    \
     V8.CompileScriptMicroSeconds.IsolateCacheHit, 1000000, MICROSECOND)       \
  HT(compile_script_with_consume_cache,                                        \
     V8.CompileScriptMicroSeconds.ConsumeCache, 1000000, MICROSECOND)          \
  HT(compile_script_consume_failed,                                            \
     V8.CompileScriptMicroSeconds.ConsumeCache.Failed, 1000000, MICROSECOND)   \
  HT(compile_script_no_cache_other,                                            \
     V8.CompileScriptMicroSeconds.NoCache.Other, 1000000, MICROSECOND)         \
  HT(compile_script_no_cache_because_inline_script,                            \
     V8.CompileScriptMicroSeconds.NoCache.InlineScript, 1000000, MICROSECOND)  \
  HT(compile_script_no_cache_because_script_too_small,                         \
     V8.CompileScriptMicroSeconds.NoCache.ScriptTooSmall, 1000000,             \
     MICROSECOND)                                                              \
  HT(compile_script_no_cache_because_cache_too_cold,                           \
     V8.CompileScriptMicroSeconds.NoCache.CacheTooCold, 1000000, MICROSECOND)  \
  HT(compile_script_streaming_finalization,                                    \
     V8.CompileScriptMicroSeconds.StreamingFinalization, 1000000, MICROSECOND) \
  HT(compile_script_on_background,                                             \
     V8.CompileScriptMicroSeconds.BackgroundThread, 1000000, MICROSECOND)      \
  HT(compile_function_on_background,                                           \
     V8.CompileFunctionMicroSeconds.BackgroundThread, 1000000, MICROSECOND)    \
  HT(wasm_max_lazy_compilation_time_5sec,                                      \
     V8.WasmMaxLazyCompilationTime5SecMilliSeconds, 5000, MILLISECOND)         \
  HT(wasm_max_lazy_compilation_time_20sec,                                     \
     V8.WasmMaxLazyCompilationTime20SecMilliSeconds, 5000, MILLISECOND)        \
  HT(wasm_max_lazy_compilation_time_60sec,                                     \
     V8.WasmMaxLazyCompilationTime60SecMilliSeconds, 5000, MILLISECOND)        \
  HT(wasm_max_lazy_compilation_time_120sec,                                    \
     V8.WasmMaxLazyCompilationTime120SecMilliSeconds, 5000, MILLISECOND)       \
  HT(wasm_sum_lazy_compilation_time_5sec,                                      \
     V8.WasmSumLazyCompilationTime5SecMilliSeconds, 20000, MILLISECOND)        \
  HT(wasm_sum_lazy_compilation_time_20sec,                                     \
     V8.WasmSumLazyCompilationTime20SecMilliSeconds, 20000, MILLISECOND)       \
  HT(wasm_sum_lazy_compilation_time_60sec,                                     \
     V8.WasmSumLazyCompilationTime60SecMilliSeconds, 20000, MILLISECOND)       \
  HT(wasm_sum_lazy_compilation_time_120sec,                                    \
     V8.WasmSumLazyCompilationTime120SecMilliSeconds, 20000, MILLISECOND)

#define AGGREGATABLE_HISTOGRAM_TIMER_LIST(AHT) \
  AHT(compile_lazy, V8.CompileLazyMicroSeconds)

#define HISTOGRAM_PERCENTAGE_LIST(HP)                                          \
  /* Heap fragmentation. */                                                    \
  HP(external_fragmentation_total, V8.MemoryExternalFragmentationTotal)        \
  HP(external_fragmentation_old_space, V8.MemoryExternalFragmentationOldSpace) \
  HP(external_fragmentation_code_space,                                        \
     V8.MemoryExternalFragmentationCodeSpace)                                  \
  HP(external_fragmentation_map_space, V8.MemoryExternalFragmentationMapSpace) \
  HP(external_fragmentation_lo_space, V8.MemoryExternalFragmentationLoSpace)

// Note: These use Histogram with options (min=1000, max=500000, buckets=50).
#define HISTOGRAM_LEGACY_MEMORY_LIST(HM)                                      \
  HM(heap_sample_total_committed, V8.MemoryHeapSampleTotalCommitted)          \
  HM(heap_sample_total_used, V8.MemoryHeapSampleTotalUsed)                    \
  HM(heap_sample_map_space_committed, V8.MemoryHeapSampleMapSpaceCommitted)   \
  HM(heap_sample_code_space_committed, V8.MemoryHeapSampleCodeSpaceCommitted) \
  HM(heap_sample_maximum_committed, V8.MemoryHeapSampleMaximumCommitted)

// WARNING: STATS_COUNTER_LIST_* is a very large macro that is causing MSVC
// Intellisense to crash.  It was broken into two macros (each of length 40
// lines) rather than one macro (of length about 80 lines) to work around
// this problem.  Please avoid using recursive macros of this length when
// possible.
#define STATS_COUNTER_LIST_1(SC)                                     \
  /* Global Handle Count*/                                           \
  SC(global_handles, V8.GlobalHandles)                               \
  SC(alive_after_last_gc, V8.AliveAfterLastGC)                       \
  SC(compilation_cache_hits, V8.CompilationCacheHits)                \
  SC(compilation_cache_misses, V8.CompilationCacheMisses)            \
  /* Number of times the cache contained a reusable Script but not   \
     the root SharedFunctionInfo */                                  \
  SC(compilation_cache_partial_hits, V8.CompilationCachePartialHits) \
  SC(objs_since_last_young, V8.ObjsSinceLastYoung)                   \
  SC(objs_since_last_full, V8.ObjsSinceLastFull)

#define STATS_COUNTER_LIST_2(SC)                                               \
  SC(gc_compactor_caused_by_request, V8.GCCompactorCausedByRequest)            \
  SC(gc_compactor_caused_by_promoted_data, V8.GCCompactorCausedByPromotedData) \
  SC(gc_compactor_caused_by_oldspace_exhaustion,                               \
     V8.GCCompactorCausedByOldspaceExhaustion)                                 \
  SC(enum_cache_hits, V8.EnumCacheHits)                                        \
  SC(enum_cache_misses, V8.EnumCacheMisses)                                    \
  SC(megamorphic_stub_cache_updates, V8.MegamorphicStubCacheUpdates)           \
  SC(regexp_entry_runtime, V8.RegExpEntryRuntime)                              \
  SC(stack_interrupts, V8.StackInterrupts)                                     \
  SC(new_space_bytes_available, V8.MemoryNewSpaceBytesAvailable)               \
  SC(new_space_bytes_committed, V8.MemoryNewSpaceBytesCommitted)               \
  SC(new_space_bytes_used, V8.MemoryNewSpaceBytesUsed)                         \
  SC(old_space_bytes_available, V8.MemoryOldSpaceBytesAvailable)               \
  SC(old_space_bytes_committed, V8.MemoryOldSpaceBytesCommitted)               \
  SC(old_space_bytes_used, V8.MemoryOldSpaceBytesUsed)                         \
  SC(code_space_bytes_available, V8.MemoryCodeSpaceBytesAvailable)             \
  SC(code_space_bytes_committed, V8.MemoryCodeSpaceBytesCommitted)             \
  SC(code_space_bytes_used, V8.MemoryCodeSpaceBytesUsed)                       \
  SC(map_space_bytes_available, V8.MemoryMapSpaceBytesAvailable)               \
  SC(map_space_bytes_committed, V8.MemoryMapSpaceBytesCommitted)               \
  SC(map_space_bytes_used, V8.MemoryMapSpaceBytesUsed)                         \
  SC(lo_space_bytes_available, V8.MemoryLoSpaceBytesAvailable)                 \
  SC(lo_space_bytes_committed, V8.MemoryLoSpaceBytesCommitted)                 \
  SC(lo_space_bytes_used, V8.MemoryLoSpaceBytesUsed)                           \
  SC(wasm_generated_code_size, V8.WasmGeneratedCodeBytes)                      \
  SC(wasm_reloc_size, V8.WasmRelocBytes)                                       \
  SC(wasm_lazily_compiled_functions, V8.WasmLazilyCompiledFunctions)

// List of counters that can be incremented from generated code. We need them in
// a separate list to be able to relocate them.
#define STATS_COUNTER_NATIVE_CODE_LIST(SC)                         \
  /* Number of write barriers executed at runtime. */              \
  SC(write_barriers, V8.WriteBarriers)                             \
  SC(regexp_entry_native, V8.RegExpEntryNative)                    \
  SC(megamorphic_stub_cache_probes, V8.MegamorphicStubCacheProbes) \
  SC(megamorphic_stub_cache_misses, V8.MegamorphicStubCacheMisses)

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_COUNTERS_DEFINITIONS_H_
