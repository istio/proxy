/**
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

mergeInto(LibraryManager.library, {
  proxy_get_configuration: function() {},
  proxy_log: function() {},
  proxy_get_log_level: function() {},
  proxy_set_tick_period_milliseconds: function() {},
  proxy_get_current_time_nanoseconds: function() {},
  proxy_get_status: function() {},
  proxy_get_property: function() {},
  proxy_set_property: function() {},
  proxy_continue_stream: function() {},
  proxy_close_stream: function() {},
  proxy_clear_route_cache: function() {},
  proxy_add_header_map_value: function() {},
  proxy_get_header_map_value: function() {},
  proxy_get_header_map_pairs: function() {},
  proxy_set_header_map_pairs: function() {},
  proxy_get_header_map_size: function() {},
  proxy_get_shared_data: function() {},
  proxy_set_shared_data: function() {},
  proxy_register_shared_queue: function() {},
  proxy_resolve_shared_queue: function() {},
  proxy_enqueue_shared_queue: function() {},
  proxy_dequeue_shared_queue: function() {},
  proxy_replace_header_map_value: function() {},
  proxy_remove_header_map_value: function() {},
  proxy_get_buffer_bytes: function() {},
  proxy_get_buffer_status: function() {},
  proxy_set_buffer_bytes: function() {},
  proxy_http_call: function() {},
  proxy_define_metric: function() {},
  proxy_increment_metric: function() {},
  proxy_record_metric: function() {},
  proxy_get_metric: function() {},
  proxy_grpc_call: function() {},
  proxy_grpc_stream: function() {},
  proxy_grpc_send: function() {},
  proxy_grpc_close: function() {},
  proxy_grpc_cancel: function() {},
  proxy_send_local_response: function() {},
  proxy_set_effective_context: function() {},
  proxy_done: function() {},
  proxy_call_foreign_function: function() {},
});
