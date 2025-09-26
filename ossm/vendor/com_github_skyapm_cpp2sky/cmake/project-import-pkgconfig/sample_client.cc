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

#include <chrono>
#include <string>

#include "cpp2sky/propagation.h"
#include "cpp2sky/tracer.h"
#include "cpp2sky/tracing_context.h"
#include "cpp2sky/well_known_names.h"
#include "httplib.h"

using namespace cpp2sky;

TracerConfig config;

void init() {
  config.set_instance_name("client_0");
  config.set_service_name("");
  config.set_address("0.0.0.0:11800");
}

int main() {
  init();

  // 1. Create tracer object to send span data to OAP.
  auto tracer = createInsecureGrpcTracer(config);

  // 2. Create tracing context
  auto tracing_context = tracer->newContext();

  /**
   * 3. Create entry span it traces RPC call.
   * Span lifetime is managed by RAII. So user don't have to call startSpan and
   * endSpan explicitly. But it provides basic approach that doesn't use RAII.
   *
   * example:
   *
   * auto current_span = tracing_context->createEntrySpan();
   * current_span->startSpan("sample_op1");
   *
   * auto current_span2 = tracing_context->createExitSpan();
   * current_span2->startSpan("sample_op2");
   *
   * httplib::Client cli("remote", 8082);
   * httplib::Headers headers = {
   *   {kPropagationHeader.data(),
   *   tracing_context->createSW8HeaderValue("remote:8082")}};
   *
   * auto res = cli.Get("/ping", headers);
   *
   * current_span2->endSpan();
   * current_span->endSpan();
   *
   */
  {
    StartEntrySpan entry_span(tracing_context, "sample_op1");

    {
      std::string target_address = "127.0.0.1:8081";
      StartExitSpan exit_span(tracing_context, entry_span.get(), "sample_op2");
      exit_span.get()->setPeer(target_address);

      httplib::Client cli("127.0.0.1", 8081);
      httplib::Headers headers = {
          {kPropagationHeader.data(),
           *tracing_context->createSW8HeaderValue(target_address)}};

      auto res = cli.Get("/ping", headers);
    }
  }

  tracer->report(std::move(tracing_context));
  return 0;
}
