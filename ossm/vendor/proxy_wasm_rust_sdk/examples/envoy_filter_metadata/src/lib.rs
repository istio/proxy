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

use proxy_wasm::traits::*;
use proxy_wasm::types::*;

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_http_context(|_, _| -> Box<dyn HttpContext> { Box::new(MetadataHttp {}) });
}}

struct MetadataHttp {}

impl Context for MetadataHttp {}

impl HttpContext for MetadataHttp {
    fn on_http_request_headers(&mut self, _: usize, _: bool) -> Action {
        // Read data set by the lua filter
        match self.get_property(vec![
            "metadata",
            "filter_metadata",
            "envoy.filters.http.lua",
            "uppercased-custom-metadata",
        ]) {
            Some(metadata) => match String::from_utf8(metadata) {
                Ok(data) => {
                    self.send_http_response(
                        200,
                        vec![("Powered-By", "proxy-wasm"), ("uppercased-metadata", &data)],
                        Some(format!("Custom response with Envoy metadata: {data:?}\n").as_bytes()),
                    );
                    Action::Pause
                }
                _ => Action::Continue,
            },
            _ => Action::Continue,
        }
    }
}
