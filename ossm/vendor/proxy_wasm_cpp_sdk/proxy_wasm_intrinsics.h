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
 * API Available to WASM modules.
 */
// NOLINT(namespace-envoy)
#pragma once

#ifndef PROXY_WASM_KEEPALIVE
#define PROXY_WASM_KEEPALIVE __attribute__((used)) __attribute__((visibility("default")))
#endif
#define WASM_EXPORT(_return_type, _function_name, _function_args)                                  \
  extern "C" PROXY_WASM_KEEPALIVE _return_type _function_name _function_args
#define START_WASM_PLUGIN(_x)
#define END_WASM_PLUGIN

#include <cstdint>
#include <string_view>
#include <optional>

#include "proxy_wasm_enums.h"
#include "proxy_wasm_common.h"
#include "proxy_wasm_externs.h"
#ifdef PROXY_WASM_PROTOBUF_FULL
#define PROXY_WASM_PROTOBUF 1
#include "proxy_wasm_intrinsics.pb.h"
#endif
#ifdef PROXY_WASM_PROTOBUF_LITE
#define PROXY_WASM_PROTOBUF 1
#include "proxy_wasm_intrinsics_lite.pb.h"
#endif
#include "proxy_wasm_api.h"
