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
  config.set_service_name("consumer");
  config.set_address("collector:19876");
}

int main() {
  init();

  httplib::Server svr;
  auto tracer = createInsecureGrpcTracer(config);

  // C++
  svr.Get("/ping", [&](const httplib::Request& req, httplib::Response& res) {
    auto tracing_context = tracer->newContext();

    {
      StartEntrySpan entry_span(tracing_context, "/ping");

      {
        std::string target_address = "provider:8081";

        StartExitSpan exit_span(tracing_context, entry_span.get(), "/pong");
        exit_span.get()->setPeer(target_address);

        httplib::Client cli("provider", 8081);
        httplib::Headers headers = {
            {kPropagationHeader.data(),
             *tracing_context->createSW8HeaderValue(target_address)}};
        auto res = cli.Get("/pong", headers);
      }
    }

    tracer->report(std::move(tracing_context));
  });

  // Python
  svr.Get("/ping2", [&](const httplib::Request& req, httplib::Response& res) {
    auto tracing_context = tracer->newContext();

    {
      StartEntrySpan entry_span(tracing_context, "/ping2");

      {
        std::string target_address = "bridge:8082";

        StartExitSpan exit_span(tracing_context, entry_span.get(), "/users");
        exit_span.get()->setPeer(target_address);

        httplib::Client cli("bridge", 8082);
        httplib::Headers headers = {
            {kPropagationHeader.data(),
             *tracing_context->createSW8HeaderValue(target_address)}};
        auto res = cli.Get("/users", headers);
      }
    }

    tracer->report(std::move(tracing_context));
  });

  svr.listen("0.0.0.0", 8080);
  return 0;
}
