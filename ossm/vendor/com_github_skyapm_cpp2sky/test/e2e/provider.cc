
// Copyright 2020 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "cpp2sky/propagation.h"
#include "cpp2sky/tracer.h"
#include "cpp2sky/tracing_context.h"
#include "cpp2sky/well_known_names.h"
#include "httplib.h"

using namespace cpp2sky;

TracerConfig config;

void init() {
  config.set_instance_name("node_0");
  config.set_service_name("provider");
  config.set_address("collector:19876");
}

int main() {
  init();

  httplib::Server svr;
  auto tracer = createInsecureGrpcTracer(config);

  svr.Get("/pong", [&](const httplib::Request& req, httplib::Response& res) {
    auto parent = req.get_header_value(kPropagationHeader.data());
    auto tracing_context = tracer->newContext(createSpanContext(parent));

    { StartEntrySpan entry_span(tracing_context, "/pong"); }

    tracer->report(std::move(tracing_context));
  });

  svr.Get("/pong2", [&](const httplib::Request& req, httplib::Response& res) {
    auto parent = req.get_header_value(kPropagationHeader.data());
    auto tracing_context = tracer->newContext(createSpanContext(parent));

    { StartEntrySpan entry_span(tracing_context, "/pong2"); }

    tracer->report(std::move(tracing_context));
  });

  svr.listen("0.0.0.0", 8081);
  return 0;
}
