#include <curl/curl.h>
#include <datadog/curl.h>
#include <datadog/dict_reader.h>
#include <datadog/error.h>
#include <datadog/optional.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <chrono>
#include <exception>
#include <system_error>
#include <unordered_set>

#include "datadog/clock.h"
#include "mocks/loggers.h"
#include "test.h"

using namespace datadog::tracing;

class SingleRequestMockCurlLibrary : public CurlLibrary {
 public:
  void *user_data_on_header_ = nullptr;
  HeaderCallback on_header_ = nullptr;
  void *user_data_on_write_ = nullptr;
  WriteCallback on_write_ = nullptr;
  CURL *added_handle_ = nullptr;
  CURLMsg message_;
  // Since `SingleRequestMockCurlLibrary` supports at most one request,
  // `created_handles_` and `destroyed_handles_` will have size zero or one.
  std::unordered_multiset<CURL *> created_handles_;
  std::unordered_multiset<CURL *> destroyed_handles_;
  // `message_result_` is the success/error code associated with the "done"
  // message sent to the event loop when the request has finished.
  CURLcode message_result_ = CURLE_OK;
  // `delay_message_` is used to prevent the immediate dispatch of a "done"
  // message to the event loop. This allows races to be explored between request
  // registration and `Curl` shutdown.
  bool delay_message_ = false;

  void easy_cleanup(CURL *handle) override {
    destroyed_handles_.insert(handle);
    CurlLibrary::easy_cleanup(handle);
  }

  CURL *easy_init() override {
    CURL *handle = CurlLibrary::easy_init();
    created_handles_.insert(handle);
    return handle;
  }

  CURLcode easy_getinfo_response_code(CURL *, long *code) override {
    *code = 200;
    return CURLE_OK;
  }
  CURLcode easy_setopt_headerdata(CURL *, void *data) override {
    user_data_on_header_ = data;
    return CURLE_OK;
  }
  CURLcode easy_setopt_headerfunction(CURL *,
                                      HeaderCallback on_header) override {
    on_header_ = on_header;
    return CURLE_OK;
  }
  CURLcode easy_setopt_writedata(CURL *, void *data) override {
    user_data_on_write_ = data;
    return CURLE_OK;
  }

  CURLcode easy_setopt_writefunction(CURL *, WriteCallback on_write) override {
    on_write_ = on_write;
    return CURLE_OK;
  }

  CURLcode easy_setopt_timeout_ms(CURL *, long) override { return CURLE_OK; }

  CURLMcode multi_add_handle(CURLM *, CURL *easy_handle) override {
    added_handle_ = easy_handle;
    return CURLM_OK;
  }
  CURLMsg *multi_info_read(CURLM *, int *msgs_in_queue) override {
    if (delay_message_) {
      *msgs_in_queue = 0;
      return nullptr;
    }

    *msgs_in_queue = added_handle_ != nullptr;
    if (*msgs_in_queue == 0) {
      return nullptr;
    }
    message_.msg = CURLMSG_DONE;
    message_.easy_handle = added_handle_;
    message_.data.result = message_result_;
    return &message_;
  }
  CURLMcode multi_perform(CURLM *, int *running_handles) override {
    if (!added_handle_) {
      *running_handles = 0;
      return CURLM_OK;
    }

    // If any of these `REQUIRE`s fail, an exception will be thrown and the
    // test will abort. The runtime will print the exception first, though.
    REQUIRE(on_header_);
    REQUIRE(user_data_on_header_);
    *running_handles = 1;
    std::string header = "200 OK";
    REQUIRE(on_header_(header.data(), 1, header.size(), user_data_on_header_) ==
            header.size());
    header = "Foo-Bar: baz";
    REQUIRE(on_header_(header.data(), 1, header.size(), user_data_on_header_) ==
            header.size());
    header = "BOOM-BOOM: boom, boom, boom, boom    ";
    REQUIRE(on_header_(header.data(), 1, header.size(), user_data_on_header_) ==
            header.size());
    header = "BOOM-boom: ignored";
    REQUIRE(on_header_(header.data(), 1, header.size(), user_data_on_header_) ==
            header.size());

    REQUIRE(on_write_);
    REQUIRE(user_data_on_write_);
    std::string body = "{\"message\": \"Dogs don't know it's not libcurl!\"}";
    // Send the body in two pieces.
    REQUIRE(on_write_(body.data(), 1, body.size() / 2, user_data_on_write_) ==
            body.size() / 2);
    const auto remaining = body.size() - (body.size() / 2);
    REQUIRE(on_write_(body.data() + body.size() / 2, 1, remaining,
                      user_data_on_write_) == remaining);

    return CURLM_OK;
  }
  CURLMcode multi_remove_handle(CURLM *, CURL *easy_handle) override {
    REQUIRE(easy_handle == added_handle_);
    added_handle_ = nullptr;
    return CURLM_OK;
  }
};

TEST_CASE("parse response headers and body", "[curl]") {
  const auto clock = default_clock;
  const auto logger = std::make_shared<MockLogger>();
  SingleRequestMockCurlLibrary library;
  const auto client = std::make_shared<Curl>(logger, clock, library);

  SECTION("in the tracer") {
    // The tracer doesn't read response headers, at least as of this writing.
    // It's still good to test that everything works with this mock
    // `CurlLibrary` in place, though.
    TracerConfig config;
    config.service = "testsvc";
    config.logger = logger;
    config.agent.http_client = client;
    // The http client is a mock that only expects a single request, so
    // force only tracing to be sent and exclude telemetry.
    config.report_telemetry = false;

    const auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};

    (void)tracer.create_span();
    // The rest runs as everything in this scope is destroyed.
  }

  SECTION("by hand") {
    // Without using a tracer, just make a request using `Curl::post`, and
    // verify that the received response headers are as expected.
    Optional<Error> post_error;
    std::exception_ptr exception;
    const HTTPClient::URL url = {"http", "whatever", ""};
    const auto result = client->post(
        url, [](const auto &) {}, "whatever",
        [&](int status, const DictReader &headers, std::string body) {
          try {
            REQUIRE(status == 200);
            REQUIRE(headers.lookup("foo-bar") == "baz");
            REQUIRE(headers.lookup("boom-boom") == "boom, boom, boom, boom");
            REQUIRE_FALSE(headers.lookup("snafu"));
            headers.visit([](StringView key, StringView value) {
              if (key == "foo-bar") {
                REQUIRE(value == "baz");
              } else {
                REQUIRE(key == "boom-boom");
                REQUIRE(value == "boom, boom, boom, boom");
              }
            });

            REQUIRE(body ==
                    "{\"message\": \"Dogs don't know it's not libcurl!\"}");
          } catch (...) {
            exception = std::current_exception();
          }
        },
        [&](const Error &error) { post_error = error; },
        clock().tick + std::chrono::seconds(10));

    REQUIRE(result);
    client->drain(clock().tick + std::chrono::seconds(1));
    if (exception) {
      std::rethrow_exception(exception);
    }
    REQUIRE_FALSE(post_error);
  }
}

TEST_CASE("bad multi-handle means error mode", "[curl]") {
  // If libcurl fails to allocate a multi-handle, then the HTTP client enters a
  // mode where calls to `post` always return an error.
  class MockCurlLibrary : public CurlLibrary {
    CURLM *multi_init() override { return nullptr; }
  };

  const auto clock = default_clock;
  const auto logger = std::make_shared<MockLogger>();
  MockCurlLibrary library;
  const auto client = std::make_shared<Curl>(logger, clock, library);
  REQUIRE(logger->first_error().code == Error::CURL_HTTP_CLIENT_SETUP_FAILED);

  const auto ignore = [](auto &&...) {};
  const HTTPClient::URL url = {"http", "whatever", ""};
  const auto dummy_deadline = clock().tick + std::chrono::seconds(10);
  const auto result =
      client->post(url, ignore, "dummy body", ignore, ignore, dummy_deadline);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == Error::CURL_HTTP_CLIENT_NOT_RUNNING);
}

TEST_CASE("bad std::thread means error mode", "[curl]") {
  // If `Curl` is unable to start its event loop thread, then it enters a mode
  // where calls to `post` always return an error.
  const auto clock = default_clock;
  const auto logger = std::make_shared<MockLogger>();
  CurlLibrary libcurl;  // the default implementation
  const auto client = std::make_shared<Curl>(
      logger, clock, libcurl, [](auto &&) -> std::thread {
        throw std::system_error(
            std::make_error_code(std::errc::resource_unavailable_try_again));
      });
  REQUIRE(logger->first_error().code == Error::CURL_HTTP_CLIENT_SETUP_FAILED);

  const auto ignore = [](auto &&...) {};
  const auto dummy_deadline = clock().tick + std::chrono::seconds(10);
  const HTTPClient::URL url = {"http", "whatever", ""};
  const auto result =
      client->post(url, ignore, "dummy body", ignore, ignore, dummy_deadline);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == Error::CURL_HTTP_CLIENT_NOT_RUNNING);
}

TEST_CASE("fail to allocate request handle", "[curl]") {
  // Each call to `Curl::post` allocates a new "easy handle."  If that fails,
  // then `post` immediately returns an error.
  class MockCurlLibrary : public CurlLibrary {
   public:
    CURL *easy_init() override { return nullptr; }
  };

  const auto clock = default_clock;
  const auto logger = std::make_shared<NullLogger>();
  MockCurlLibrary library;
  const auto client = std::make_shared<Curl>(logger, clock, library);

  const auto ignore = [](auto &&...) {};
  const HTTPClient::URL url = {"http", "whatever", ""};
  const auto dummy_deadline = clock().tick + std::chrono::seconds(10);
  const auto result =
      client->post(url, ignore, "dummy body", ignore, ignore, dummy_deadline);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == Error::CURL_REQUEST_SETUP_FAILED);
}

TEST_CASE("setopt failures", "[curl]") {
  // Each call to `Curl::post` allocates a new "easy handle" and sets various
  // options on it.  Any of those setters can fail.  When one does, `post`
  // immediately returns an error.
  class MockCurlLibrary : public CurlLibrary {
   public:
    CURLoption fail = CURLOPT_LASTENTRY;
    CURLcode error = CURLE_OUT_OF_MEMORY;

    CURLcode easy_setopt_errorbuffer(CURL *, char *) override {
      if (fail == CURLOPT_ERRORBUFFER) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_headerdata(CURL *, void *) override {
      if (fail == CURLOPT_HEADERDATA) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_headerfunction(CURL *, HeaderCallback) override {
      if (fail == CURLOPT_HEADERFUNCTION) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_httpheader(CURL *, curl_slist *) override {
      if (fail == CURLOPT_HTTPHEADER) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_post(CURL *, long) override {
      if (fail == CURLOPT_POST) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_postfields(CURL *, const char *) override {
      if (fail == CURLOPT_POSTFIELDS) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_postfieldsize(CURL *, long) override {
      if (fail == CURLOPT_POSTFIELDSIZE) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_private(CURL *, void *) override {
      if (fail == CURLOPT_PRIVATE) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_unix_socket_path(CURL *, const char *) override {
      if (fail == CURLOPT_UNIX_SOCKET_PATH) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_url(CURL *, const char *) override {
      if (fail == CURLOPT_URL) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_writedata(CURL *, void *) override {
      if (fail == CURLOPT_WRITEDATA) {
        return error;
      }
      return CURLE_OK;
    }
    CURLcode easy_setopt_writefunction(CURL *, WriteCallback) override {
      if (fail == CURLOPT_WRITEFUNCTION) {
        return error;
      }
      return CURLE_OK;
    }
  };

  struct TestCase {
    CURLoption which_fails;
    std::string name;
  };

#define CASE(OPTION) \
  { OPTION, #OPTION }

  const auto &[which_fails, name] = GENERATE(
      values<TestCase>({CASE(CURLOPT_ERRORBUFFER), CASE(CURLOPT_HEADERDATA),
                        CASE(CURLOPT_HEADERFUNCTION), CASE(CURLOPT_HTTPHEADER),
                        CASE(CURLOPT_POST), CASE(CURLOPT_POSTFIELDS),
                        CASE(CURLOPT_POSTFIELDSIZE), CASE(CURLOPT_PRIVATE),
                        CASE(CURLOPT_UNIX_SOCKET_PATH), CASE(CURLOPT_URL),
                        CASE(CURLOPT_WRITEDATA), CASE(CURLOPT_WRITEFUNCTION)}));

#undef CASE

  CAPTURE(name);
  MockCurlLibrary library;
  library.fail = which_fails;

  const auto clock = default_clock;
  const auto logger = std::make_shared<NullLogger>();
  const auto client = std::make_shared<Curl>(logger, clock, library);

  const auto ignore = [](auto &&...) {};
  HTTPClient::URL url;
  if (which_fails == CURLOPT_UNIX_SOCKET_PATH) {
    url.scheme = "unix";
    url.path = "/foo/bar.sock";
  } else {
    url.scheme = "http";
    url.authority = "localhost";
    url.path = "/trace/thing";
  }

  const auto dummy_deadline = clock().tick + std::chrono::seconds(10);
  const auto result =
      client->post(url, ignore, "dummy body", ignore, ignore, dummy_deadline);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == Error::CURL_REQUEST_SETUP_FAILED);
}

TEST_CASE("handles are always cleaned up", "[curl]") {
  const auto clock = default_clock;
  const auto logger = std::make_shared<MockLogger>();
  SingleRequestMockCurlLibrary library;
  auto client = std::make_shared<Curl>(logger, clock, library);

  SECTION("when the response is delivered") {
    Optional<Error> post_error;
    std::exception_ptr exception;
    const HTTPClient::URL url = {"http", "whatever", ""};
    const auto dummy_deadline = clock().tick + std::chrono::seconds(10);
    const auto result = client->post(
        url, [](const auto &) {}, "whatever",
        [&](int status, const DictReader & /*headers*/, std::string body) {
          try {
            REQUIRE(status == 200);
            REQUIRE(body ==
                    "{\"message\": \"Dogs don't know it's not libcurl!\"}");
          } catch (...) {
            exception = std::current_exception();
          }
        },
        [&](const Error &error) { post_error = error; }, dummy_deadline);

    REQUIRE(result);
    client->drain(clock().tick + std::chrono::seconds(1));
    if (exception) {
      std::rethrow_exception(exception);
    }
    REQUIRE_FALSE(post_error);
  }

  SECTION("when an error occurs") {
    Optional<Error> post_error;
    const HTTPClient::URL url = {"http", "whatever", ""};
    const auto ignore = [](auto &&...) {};
    const auto dummy_deadline = clock().tick + std::chrono::seconds(10);
    library.message_result_ = CURLE_COULDNT_CONNECT;  // any error would do
    const auto result = client->post(
        url, ignore, "whatever", ignore,
        [&](const Error &error) { post_error = error; }, dummy_deadline);

    REQUIRE(result);
    client->drain(clock().tick + std::chrono::seconds(1));
    REQUIRE(post_error);
  }

  SECTION("when we shut down while a request is in flight") {
    const HTTPClient::URL url = {"http", "whatever", ""};
    const auto ignore = [](auto &&...) {};
    const auto dummy_deadline = clock().tick + std::chrono::seconds(10);
    library.delay_message_ = true;
    const auto result =
        client->post(url, ignore, "whatever", ignore, ignore, dummy_deadline);

    REQUIRE(result);
    // Destroy the `Curl` object.
    client.reset();
  }

  // Here are the checks relevant to this test.
  REQUIRE(library.created_handles_.size() == 1);
  REQUIRE(library.created_handles_ == library.destroyed_handles_);
}

TEST_CASE("post() deadline exceeded before request start", "[curl]") {
  const auto clock = default_clock;
  Curl client{std::make_shared<NullLogger>(), clock};

  const auto ignore = [](auto &&...) {};
  const HTTPClient::URL url = {"http", "whatever", ""};
  const std::string body;
  const auto deadline = clock().tick - std::chrono::milliseconds(1);
  Optional<Error> error_delivered;

  const auto result = client.post(
      url, ignore, body, ignore, [&](Error error) { error_delivered = error; },
      deadline);
  REQUIRE(result);

  client.drain(clock().tick + std::chrono::seconds(1));

  REQUIRE(error_delivered);
  REQUIRE(error_delivered->code ==
          Error::CURL_DEADLINE_EXCEEDED_BEFORE_REQUEST_START);
}
