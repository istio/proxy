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
 * Proxy-WASM ABI.
 */
// NOLINT(namespace-envoy)
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "proxy_wasm_common.h"
#include "proxy_wasm_enums.h"

//
// ABI
//

// Configuration and Status
extern "C" WasmResult proxy_get_configuration(const char **configuration_ptr,
                                              size_t *configuration_size);
// Results status details for any previous ABI call and onGrpcClose.
extern "C" WasmResult proxy_get_status(uint32_t *status_code_ptr, const char **message_ptr,
                                       size_t *message_size);

// Logging
extern "C" WasmResult proxy_log(LogLevel level, const char *logMessage, size_t messageSize);
extern "C" WasmResult proxy_get_log_level(LogLevel *level);

// Timer (will be set for the root context, e.g. onStart, onTick).
extern "C" WasmResult proxy_set_tick_period_milliseconds(uint32_t millisecond);

// Time
extern "C" WasmResult proxy_get_current_time_nanoseconds(uint64_t *nanoseconds);

// State accessors
extern "C" WasmResult proxy_get_property(const char *path_ptr, size_t path_size,
                                         const char **value_ptr_ptr, size_t *value_size_ptr);
extern "C" WasmResult proxy_set_property(const char *path_ptr, size_t path_size,
                                         const char *value_ptr, size_t value_size);

// Continue/Close/Reply/Route
extern "C" WasmResult proxy_continue_stream(WasmStreamType stream_type);
extern "C" WasmResult proxy_close_stream(WasmStreamType stream_type);
extern "C" WasmResult
proxy_send_local_response(uint32_t response_code, const char *response_code_details_ptr,
                          size_t response_code_details_size, const char *body_ptr, size_t body_size,
                          const char *additional_response_header_pairs_ptr,
                          size_t additional_response_header_pairs_size, uint32_t grpc_status);
extern "C" WasmResult proxy_clear_route_cache();

// SharedData
// Returns: Ok, NotFound
extern "C" WasmResult proxy_get_shared_data(const char *key_ptr, size_t key_size,
                                            const char **value_ptr, size_t *value_size,
                                            uint32_t *cas);
//  If cas != 0 and cas != the current cas for 'key' return false, otherwise set
//  the value and return true.
// Returns: Ok, CasMismatch
extern "C" WasmResult proxy_set_shared_data(const char *key_ptr, size_t key_size,
                                            const char *value_ptr, size_t value_size, uint32_t cas);

// SharedQueue
// Note: Registering the same queue_name will overwrite the old registration
// while preseving any pending data. Consequently it should typically be
// followed by a call to proxy_dequeue_shared_queue. Returns: Ok
extern "C" WasmResult proxy_register_shared_queue(const char *queue_name_ptr,
                                                  size_t queue_name_size, uint32_t *token);
// Returns: Ok, NotFound
extern "C" WasmResult proxy_resolve_shared_queue(const char *vm_id, size_t vm_id_size,
                                                 const char *queue_name_ptr, size_t queue_name_size,
                                                 uint32_t *token);
// Returns Ok, Empty, NotFound (token not registered).
extern "C" WasmResult proxy_dequeue_shared_queue(uint32_t token, const char **data_ptr,
                                                 size_t *data_size);
// Returns false if the queue was not found and the data was not enqueued.
extern "C" WasmResult proxy_enqueue_shared_queue(uint32_t token, const char *data_ptr,
                                                 size_t data_size);

// Headers/Trailers/Metadata Maps
extern "C" WasmResult proxy_add_header_map_value(WasmHeaderMapType type, const char *key_ptr,
                                                 size_t key_size, const char *value_ptr,
                                                 size_t value_size);
extern "C" WasmResult proxy_get_header_map_value(WasmHeaderMapType type, const char *key_ptr,
                                                 size_t key_size, const char **value_ptr,
                                                 size_t *value_size);
extern "C" WasmResult proxy_get_header_map_pairs(WasmHeaderMapType type, const char **ptr,
                                                 size_t *size);
extern "C" WasmResult proxy_set_header_map_pairs(WasmHeaderMapType type, const char *ptr,
                                                 size_t size);
extern "C" WasmResult proxy_replace_header_map_value(WasmHeaderMapType type, const char *key_ptr,
                                                     size_t key_size, const char *value_ptr,
                                                     size_t value_size);
extern "C" WasmResult proxy_remove_header_map_value(WasmHeaderMapType type, const char *key_ptr,
                                                    size_t key_size);
extern "C" WasmResult proxy_get_header_map_size(WasmHeaderMapType type, size_t *size);

// Buffer
extern "C" WasmResult proxy_get_buffer_bytes(WasmBufferType type, uint32_t start, uint32_t length,
                                             const char **ptr, size_t *size);
extern "C" WasmResult proxy_get_buffer_status(WasmBufferType type, size_t *length_ptr,
                                              uint32_t *flags_ptr);
extern "C" WasmResult proxy_set_buffer_bytes(WasmBufferType type, uint32_t start, uint32_t length,
                                             const char *ptr, size_t size);

// HTTP
extern "C" WasmResult proxy_http_call(const char *uri_ptr, size_t uri_size, void *header_pairs_ptr,
                                      size_t header_pairs_size, const char *body_ptr,
                                      size_t body_size, void *trailer_pairs_ptr,
                                      size_t trailer_pairs_size, uint32_t timeout_milliseconds,
                                      uint32_t *token_ptr);
// gRPC
extern "C" WasmResult proxy_grpc_call(const char *service_ptr, size_t service_size,
                                      const char *service_name_ptr, size_t service_name_size,
                                      const char *method_name_ptr, size_t method_name_size,
                                      void *initial_metadata_ptr, size_t initial_metadata_size,
                                      const char *request_ptr, size_t request_size,
                                      uint32_t timeout_milliseconds, uint32_t *token_ptr);
extern "C" WasmResult proxy_grpc_stream(const char *service_ptr, size_t service_size,
                                        const char *service_name_ptr, size_t service_name_size,
                                        const char *method_name_ptr, size_t method_name_size,
                                        void *initial_metadata, size_t initial_metadata_size,
                                        uint32_t *token_ptr);
extern "C" WasmResult proxy_grpc_cancel(uint32_t token);
extern "C" WasmResult proxy_grpc_close(uint32_t token);
extern "C" WasmResult proxy_grpc_send(uint32_t token, const char *message_ptr, size_t message_size,
                                      uint32_t end_stream);

// Metrics
extern "C" WasmResult proxy_define_metric(MetricType type, const char *name_ptr, size_t name_size,
                                          uint32_t *metric_id);
extern "C" WasmResult proxy_increment_metric(uint32_t metric_id, int64_t offset);
extern "C" WasmResult proxy_record_metric(uint32_t metric_id, uint64_t value);
extern "C" WasmResult proxy_get_metric(uint32_t metric_id, uint64_t *result);

// System
extern "C" WasmResult proxy_set_effective_context(uint32_t effective_context_id);
extern "C" WasmResult proxy_done();
extern "C" WasmResult proxy_call_foreign_function(const char *function_name,
                                                  size_t function_name_size, const char *arguments,
                                                  size_t arguments_size, char **results,
                                                  size_t *results_size);

// Calls in.
extern "C" uint32_t proxy_on_vm_start(uint32_t root_context_id, uint32_t configuration_size);
extern "C" uint32_t proxy_validate_configuration(uint32_t root_context_id,
                                                 uint32_t configuration_size);
extern "C" uint32_t proxy_on_configure(uint32_t root_context_id, uint32_t configuration_size);
extern "C" void proxy_on_tick(uint32_t root_context_id);
extern "C" void proxy_on_foreign_function(uint32_t root_context_id, uint32_t function_id,
                                          uint32_t data_size);
extern "C" void proxy_on_queue_ready(uint32_t root_context_id, uint32_t token);

// Stream calls.
extern "C" void proxy_on_context_create(uint32_t context_id, uint32_t parent_context_id);
extern "C" FilterHeadersStatus proxy_on_request_headers(uint32_t context_id, uint32_t headers,
                                                        uint32_t end_of_stream);
extern "C" FilterDataStatus proxy_on_request_body(uint32_t context_id, uint32_t body_buffer_length,
                                                  uint32_t end_of_stream);
extern "C" FilterTrailersStatus proxy_on_request_trailers(uint32_t context_id, uint32_t trailers);
extern "C" FilterMetadataStatus proxy_on_request_metadata(uint32_t context_id, uint32_t nelements);
extern "C" FilterHeadersStatus proxy_on_response_headers(uint32_t context_id, uint32_t headers,
                                                         uint32_t end_of_stream);
extern "C" FilterDataStatus proxy_on_response_body(uint32_t context_id, uint32_t body_buffer_length,
                                                   uint32_t end_of_stream);
extern "C" FilterTrailersStatus proxy_on_response_trailers(uint32_t context_id, uint32_t trailers);
extern "C" FilterMetadataStatus proxy_on_response_metadata(uint32_t context_id, uint32_t nelements);

// HTTP/gRPC.
extern "C" void proxy_on_http_call_response(uint32_t context_id, uint32_t token, uint32_t headers,
                                            uint32_t body_size, uint32_t trailers);
extern "C" void proxy_on_grpc_receive_initial_metadata(uint32_t context_id, uint32_t token,
                                                       uint32_t headers);
extern "C" void proxy_on_grpc_trailing_metadata(uint32_t context_id, uint32_t token,
                                                uint32_t trailers);
extern "C" void proxy_on_grpc_receive(uint32_t context_id, uint32_t token, uint32_t response_size);
extern "C" void proxy_on_grpc_close(uint32_t context_id, uint32_t token, uint32_t status_code);

// The stream/vm has completed.
extern "C" uint32_t proxy_on_done(uint32_t context_id);
// proxy_on_log occurs after proxy_on_done.
extern "C" void proxy_on_log(uint32_t context_id);
// The Context in the proxy has been destroyed and no further calls will be
// coming.
extern "C" void proxy_on_delete(uint32_t context_id);
