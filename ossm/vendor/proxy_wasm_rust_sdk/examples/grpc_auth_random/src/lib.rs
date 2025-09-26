// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use log::info;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::time::Duration;

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_http_context(|_, _| -> Box<dyn HttpContext> { Box::new(GrpcAuthRandom) });
}}

struct GrpcAuthRandom;

impl HttpContext for GrpcAuthRandom {
    fn on_http_request_headers(&mut self, _: usize, _: bool) -> Action {
        match self.get_http_request_header("content-type") {
            Some(value) if value.starts_with("application/grpc") => {}
            _ => {
                // Reject non-gRPC clients.
                self.send_http_response(
                    503,
                    vec![("Powered-By", "proxy-wasm")],
                    Some(b"Service accessible only to gRPC clients.\n"),
                );
                return Action::Pause;
            }
        }

        match self.get_http_request_header(":path") {
            Some(value) if value.starts_with("/grpc.reflection") => {
                // Always allow gRPC calls to the reflection API.
                Action::Continue
            }
            _ => {
                // Allow other gRPC calls based on the result of grpcbin.GRPCBin/RandomError.
                self.dispatch_grpc_call(
                    "grpcbin",
                    "grpcbin.GRPCBin",
                    "RandomError",
                    vec![],
                    None,
                    Duration::from_secs(1),
                )
                .unwrap();
                Action::Pause
            }
        }
    }

    fn on_http_response_headers(&mut self, _: usize, _: bool) -> Action {
        self.set_http_response_header("Powered-By", Some("proxy-wasm"));
        Action::Continue
    }
}

impl Context for GrpcAuthRandom {
    fn on_grpc_call_response(&mut self, _: u32, status_code: u32, _: usize) {
        if status_code % 2 == 0 {
            info!("Access granted.");
            self.resume_http_request();
        } else {
            info!("Access forbidden.");
            self.send_grpc_response(
                GrpcStatusCode::Aborted,
                Some("Aborted by Proxy-Wasm!"),
                vec![("Powered-By", b"proxy-wasm")],
            );
        }
    }
}
