// This is an HTTP server that listens on port 80 and forwards all requests to
// the the "server" host on port 80.

#include <datadog/tracer.h>

#include <csignal>
#include <iostream>
#include <optional>
#include <string_view>

#include "datadog/dict_reader.h"
#include "datadog/dict_writer.h"
#include "datadog/span.h"
#include "datadog/span_config.h"
#include "datadog/trace_segment.h"
#include "httplib.h"
#include "tracingutil.h"

namespace dd = datadog::tracing;

// `hard_stop` is installed as a signal handler for `SIGTERM`.
// For some reason, the default handler was not being called.
void hard_stop(int /*signal*/) { std::exit(0); }

int main() {
  // Set up the Datadog tracer.  See `src/datadog/tracer_config.h`.
  dd::TracerConfig config;
  config.service = "dd-trace-cpp-http-server-example-proxy";
  config.service_type = "proxy";

  // `finalize_config` validates `config` and applies any settings from
  // environment variables, such as `DD_AGENT_HOST`.
  // If the resulting configuration is valid, it will return a
  // `FinalizedTracerConfig` that can then be used to initialize a `Tracer`.
  // If the resulting configuration is invalid, then it will return an
  // `Error` that can be printed, but then no `Tracer` can be created.
  dd::Expected<dd::FinalizedTracerConfig> finalized_config =
      dd::finalize_config(config);
  if (dd::Error* error = finalized_config.if_error()) {
    std::cerr << "Error: Datadog is misconfigured. " << *error << '\n';
    return 1;
  }

  dd::Tracer tracer{*finalized_config};

  httplib::Client upstream_client("server", 80);

  // Configure the HTTP server.
  auto forward_handler = [&tracer, &upstream_client](
                             const httplib::Request& req,
                             httplib::Response& res) {
    tracingutil::HeaderReader reader(req.headers);
    auto span = tracer.extract_or_create_span(reader);
    span->set_name("forward.request");
    span->set_resource_name(req.method + " " + req.path);
    span->set_tag("network.origin.ip", req.remote_addr);
    span->set_tag("network.origin.port", std::to_string(req.remote_port));
    span->set_tag("http.url_details.path", req.target);
    span->set_tag("http.route", req.path);
    span->set_tag("http.method", req.method);

    httplib::Error er;
    httplib::Request forward_request(req);
    forward_request.path = req.target;

    tracingutil::HeaderWriter writer(forward_request.headers);
    span->inject(writer);

    upstream_client.send(forward_request, res, er);
    if (er != httplib::Error::Success) {
      res.status = 500;
      span->set_error_message(httplib::to_string(er));
      std::cerr << "Error occurred while proxying request: " << req.target
                << "\n";
    } else {
      tracingutil::HeaderReader reader(res.headers);
      auto status = span->read_sampling_delegation_response(reader);
      if (auto error = status.if_error()) {
        std::cerr << error << "\n";
      }
    }

    span->set_tag("http.status_code", std::to_string(res.status));
  };

  httplib::Server server;
  server.Get(".*", forward_handler);
  server.Post(".*", forward_handler);
  server.Put(".*", forward_handler);
  server.Options(".*", forward_handler);
  server.Patch(".*", forward_handler);
  server.Delete(".*", forward_handler);

  std::signal(SIGTERM, hard_stop);
  const int port = 80;
  std::cout << "Proxy is running on port " << port << "\n";
  server.listen("0.0.0.0", port);

  return 0;
}
