#include "curl.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "clock.h"
#include "dict_reader.h"
#include "dict_writer.h"
#include "http_client.h"
#include "json.hpp"
#include "logger.h"
#include "string_util.h"
#include "string_view.h"

namespace datadog {
namespace tracing {
namespace {

// `libcurl` is the default implementation: it calls `curl_*` functions under
// the hood.
CurlLibrary libcurl;

}  // namespace

CURL *CurlLibrary::easy_init() { return curl_easy_init(); }

void CurlLibrary::easy_cleanup(CURL *handle) { curl_easy_cleanup(handle); }

CURLcode CurlLibrary::easy_getinfo_private(CURL *curl, char **user_data) {
  return curl_easy_getinfo(curl, CURLINFO_PRIVATE, user_data);
}

CURLcode CurlLibrary::easy_getinfo_response_code(CURL *curl, long *code) {
  return curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, code);
}

CURLcode CurlLibrary::easy_setopt_errorbuffer(CURL *handle, char *buffer) {
  return curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, buffer);
}

CURLcode CurlLibrary::easy_setopt_headerdata(CURL *handle, void *data) {
  return curl_easy_setopt(handle, CURLOPT_HEADERDATA, data);
}

CURLcode CurlLibrary::easy_setopt_headerfunction(CURL *handle,
                                                 HeaderCallback on_header) {
  return curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, on_header);
}

CURLcode CurlLibrary::easy_setopt_httpheader(CURL *handle,
                                             curl_slist *headers) {
  return curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
}

CURLcode CurlLibrary::easy_setopt_post(CURL *handle, long post) {
  return curl_easy_setopt(handle, CURLOPT_POST, post);
}

CURLcode CurlLibrary::easy_setopt_postfields(CURL *handle, const char *data) {
  return curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data);
}

CURLcode CurlLibrary::easy_setopt_postfieldsize(CURL *handle, long size) {
  return curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, size);
}

CURLcode CurlLibrary::easy_setopt_private(CURL *handle, void *pointer) {
  return curl_easy_setopt(handle, CURLOPT_PRIVATE, pointer);
}

CURLcode CurlLibrary::easy_setopt_unix_socket_path(CURL *handle,
                                                   const char *path) {
  return curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, path);
}

CURLcode CurlLibrary::easy_setopt_url(CURL *handle, const char *url) {
  return curl_easy_setopt(handle, CURLOPT_URL, url);
}

CURLcode CurlLibrary::easy_setopt_writedata(CURL *handle, void *data) {
  return curl_easy_setopt(handle, CURLOPT_WRITEDATA, data);
}

CURLcode CurlLibrary::easy_setopt_writefunction(CURL *handle,
                                                WriteCallback on_write) {
  return curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, on_write);
}

CURLcode CurlLibrary::easy_setopt_timeout_ms(CURL *handle, long timeout_ms) {
  return curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, timeout_ms);
}

const char *CurlLibrary::easy_strerror(CURLcode error) {
  return curl_easy_strerror(error);
}

void CurlLibrary::global_cleanup() { curl_global_cleanup(); }

CURLcode CurlLibrary::global_init(long flags) {
  return curl_global_init(flags);
}

CURLMcode CurlLibrary::multi_add_handle(CURLM *multi_handle,
                                        CURL *easy_handle) {
  return curl_multi_add_handle(multi_handle, easy_handle);
}

CURLMcode CurlLibrary::multi_cleanup(CURLM *multi_handle) {
  return curl_multi_cleanup(multi_handle);
}

CURLMsg *CurlLibrary::multi_info_read(CURLM *multi_handle, int *msgs_in_queue) {
  return curl_multi_info_read(multi_handle, msgs_in_queue);
}

CURLM *CurlLibrary::multi_init() { return curl_multi_init(); }

CURLMcode CurlLibrary::multi_perform(CURLM *multi_handle,
                                     int *running_handles) {
  return curl_multi_perform(multi_handle, running_handles);
}

CURLMcode CurlLibrary::multi_poll(CURLM *multi_handle, curl_waitfd extra_fds[],
                                  unsigned extra_nfds, int timeout_ms,
                                  int *numfds) {
  return curl_multi_poll(multi_handle, extra_fds, extra_nfds, timeout_ms,
                         numfds);
}

CURLMcode CurlLibrary::multi_remove_handle(CURLM *multi_handle,
                                           CURL *easy_handle) {
  return curl_multi_remove_handle(multi_handle, easy_handle);
}

const char *CurlLibrary::multi_strerror(CURLMcode error) {
  return curl_multi_strerror(error);
}

CURLMcode CurlLibrary::multi_wakeup(CURLM *multi_handle) {
  return curl_multi_wakeup(multi_handle);
}

curl_slist *CurlLibrary::slist_append(curl_slist *list, const char *string) {
  return curl_slist_append(list, string);
}

void CurlLibrary::slist_free_all(curl_slist *list) {
  curl_slist_free_all(list);
}

using ErrorHandler = HTTPClient::ErrorHandler;
using HeadersSetter = HTTPClient::HeadersSetter;
using ResponseHandler = HTTPClient::ResponseHandler;
using URL = HTTPClient::URL;

class CurlImpl {
  std::mutex mutex_;
  CurlLibrary &curl_;
  const std::shared_ptr<Logger> logger_;
  Clock clock_;
  CURLM *multi_handle_;
  std::unordered_set<CURL *> request_handles_;
  std::list<CURL *> new_handles_;
  bool shutting_down_;
  int num_active_handles_;
  std::condition_variable no_requests_;
  std::thread event_loop_;

  struct Request {
    CurlLibrary *curl = nullptr;
    curl_slist *request_headers = nullptr;
    std::string request_body;
    ResponseHandler on_response;
    ErrorHandler on_error;
    char error_buffer[CURL_ERROR_SIZE] = "";
    std::unordered_map<std::string, std::string> response_headers_lower;
    std::string response_body;
    std::chrono::steady_clock::time_point deadline;

    ~Request();
  };

  class HeaderWriter : public DictWriter {
    curl_slist *list_ = nullptr;
    std::string buffer_;
    CurlLibrary &curl_;

   public:
    explicit HeaderWriter(CurlLibrary &curl);
    ~HeaderWriter();
    curl_slist *release();
    void set(StringView key, StringView value) override;
  };

  class HeaderReader : public DictReader {
    std::unordered_map<std::string, std::string> *response_headers_lower_;
    mutable std::string buffer_;

   public:
    explicit HeaderReader(
        std::unordered_map<std::string, std::string> *response_headers_lower);
    Optional<StringView> lookup(StringView key) const override;
    void visit(const std::function<void(StringView key, StringView value)>
                   &visitor) const override;
  };

  void run();
  void handle_message(const CURLMsg &, std::unique_lock<std::mutex> &);
  CURLcode log_on_error(CURLcode result);
  CURLMcode log_on_error(CURLMcode result);

  static std::size_t on_read_header(char *data, std::size_t, std::size_t length,
                                    void *user_data);
  static std::size_t on_read_body(char *data, std::size_t, std::size_t length,
                                  void *user_data);

 public:
  explicit CurlImpl(const std::shared_ptr<Logger> &, const Clock &,
                    CurlLibrary &, const Curl::ThreadGenerator &);
  ~CurlImpl();

  Expected<void> post(const URL &url, HeadersSetter set_headers,
                      std::string body, ResponseHandler on_response,
                      ErrorHandler on_error,
                      std::chrono::steady_clock::time_point deadline);

  void drain(std::chrono::steady_clock::time_point deadline);
};

namespace {

void throw_on_error(CURLcode result) {
  if (result != CURLE_OK) {
    throw result;
  }
}

}  // namespace

Curl::Curl(const std::shared_ptr<Logger> &logger, const Clock &clock)
    : Curl(logger, clock, libcurl) {}

Curl::Curl(const std::shared_ptr<Logger> &logger, const Clock &clock,
           CurlLibrary &curl)
    : Curl(logger, clock, curl,
           [](auto &&func) { return std::thread(std::move(func)); }) {}

Curl::Curl(const std::shared_ptr<Logger> &logger, const Clock &clock,
           CurlLibrary &curl, const Curl::ThreadGenerator &make_thread)
    : impl_(new CurlImpl{logger, clock, curl, make_thread}) {}

Curl::~Curl() { delete impl_; }

Expected<void> Curl::post(const URL &url, HeadersSetter set_headers,
                          std::string body, ResponseHandler on_response,
                          ErrorHandler on_error,
                          std::chrono::steady_clock::time_point deadline) {
  return impl_->post(url, set_headers, body, on_response, on_error, deadline);
}

void Curl::drain(std::chrono::steady_clock::time_point deadline) {
  impl_->drain(deadline);
}

nlohmann::json Curl::config_json() const {
  return nlohmann::json::object({{"type", "datadog::tracing::Curl"}});
}

CurlImpl::CurlImpl(const std::shared_ptr<Logger> &logger, const Clock &clock,
                   CurlLibrary &curl, const Curl::ThreadGenerator &make_thread)
    : curl_(curl),
      logger_(logger),
      clock_(clock),
      shutting_down_(false),
      num_active_handles_(0) {
  curl_.global_init(CURL_GLOBAL_ALL);
  multi_handle_ = curl_.multi_init();
  if (multi_handle_ == nullptr) {
    logger_->log_error(Error{
        Error::CURL_HTTP_CLIENT_SETUP_FAILED,
        "Unable to initialize a curl multi-handle for sending requests."});
    return;
  }

  try {
    event_loop_ = make_thread([this]() { run(); });
  } catch (const std::system_error &error) {
    logger_->log_error(
        Error{Error::CURL_HTTP_CLIENT_SETUP_FAILED, error.what()});

    // Usually the worker thread would do this, but since the thread failed to
    // start, do it here.
    (void)curl_.multi_cleanup(multi_handle_);
    curl_.global_cleanup();

    // Mark this object as not working.
    multi_handle_ = nullptr;
  }
}

CurlImpl::~CurlImpl() {
  if (multi_handle_ == nullptr) {
    // We're not running; nothing to shut down.
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    shutting_down_ = true;
  }
  log_on_error(curl_.multi_wakeup(multi_handle_));
  event_loop_.join();

  log_on_error(curl_.multi_cleanup(multi_handle_));
  curl_.global_cleanup();
}

Expected<void> CurlImpl::post(
    const HTTPClient::URL &url, HeadersSetter set_headers, std::string body,
    ResponseHandler on_response, ErrorHandler on_error,
    std::chrono::steady_clock::time_point deadline) try {
  if (multi_handle_ == nullptr) {
    return Error{Error::CURL_HTTP_CLIENT_NOT_RUNNING,
                 "Unable to send request via libcurl because the HTTP client "
                 "failed to start."};
  }

  HeaderWriter writer{curl_};
  set_headers(writer);
  auto cleanup_list = [&](auto list) { curl_.slist_free_all(list); };
  std::unique_ptr<curl_slist, decltype(cleanup_list)> headers{
      writer.release(), std::move(cleanup_list)};

  auto request = std::make_unique<Request>();

  request->curl = &curl_;
  request->request_headers = headers.get();
  request->request_body = std::move(body);
  request->on_response = std::move(on_response);
  request->on_error = std::move(on_error);
  request->deadline = std::move(deadline);

  auto cleanup_handle = [&](auto handle) { curl_.easy_cleanup(handle); };
  std::unique_ptr<CURL, decltype(cleanup_handle)> handle{
      curl_.easy_init(), std::move(cleanup_handle)};

  if (!handle) {
    return Error{Error::CURL_REQUEST_SETUP_FAILED,
                 "unable to initialize a curl handle for request sending"};
  }

  throw_on_error(
      curl_.easy_setopt_httpheader(handle.get(), request->request_headers));
  throw_on_error(curl_.easy_setopt_private(handle.get(), request.get()));
  throw_on_error(
      curl_.easy_setopt_errorbuffer(handle.get(), request->error_buffer));
  throw_on_error(curl_.easy_setopt_post(handle.get(), 1));
  throw_on_error(curl_.easy_setopt_postfieldsize(
      handle.get(), static_cast<long>(request->request_body.size())));
  throw_on_error(
      curl_.easy_setopt_postfields(handle.get(), request->request_body.data()));
  throw_on_error(
      curl_.easy_setopt_headerfunction(handle.get(), &on_read_header));
  throw_on_error(curl_.easy_setopt_headerdata(handle.get(), request.get()));
  throw_on_error(curl_.easy_setopt_writefunction(handle.get(), &on_read_body));
  throw_on_error(curl_.easy_setopt_writedata(handle.get(), request.get()));
  if (url.scheme == "unix" || url.scheme == "http+unix" ||
      url.scheme == "https+unix") {
    throw_on_error(curl_.easy_setopt_unix_socket_path(handle.get(),
                                                      url.authority.c_str()));
    // The authority section of the URL is ignored when a unix domain socket is
    // to be used.
    throw_on_error(curl_.easy_setopt_url(
        handle.get(), ("http://localhost" + url.path).c_str()));
  } else {
    throw_on_error(curl_.easy_setopt_url(
        handle.get(), (url.scheme + "://" + url.authority + url.path).c_str()));
  }

  std::list<CURL *> node;
  node.push_back(handle.get());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    new_handles_.splice(new_handles_.end(), node);

    (void)headers.release();
    (void)handle.release();
    (void)request.release();
  }

  log_on_error(curl_.multi_wakeup(multi_handle_));

  return nullopt;
} catch (CURLcode error) {
  return Error{Error::CURL_REQUEST_SETUP_FAILED, curl_.easy_strerror(error)};
}

void CurlImpl::drain(std::chrono::steady_clock::time_point deadline) {
  std::unique_lock<std::mutex> lock(mutex_);
  no_requests_.wait_until(lock, deadline, [this]() {
    return num_active_handles_ == 0 && new_handles_.empty();
  });
}

std::size_t CurlImpl::on_read_header(char *data, std::size_t,
                                     std::size_t length, void *user_data) {
  const auto request = static_cast<Request *>(user_data);
  // The idea is:
  //
  //         "    Foo-Bar  :   thingy, thingy, thing   \r\n"
  //    -> {"foo-bar", "thingy, thingy, thing"}
  //
  // There isn't always a colon.  Inputs without a colon can be ignored:
  //
  // > For an HTTP transfer, the status line and the blank line preceding the
  // > response body are both included as headers and passed to this
  // > function.
  //
  // https://curl.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
  //

  StringView sv(data, length);
  auto colon_idx = sv.find(":");
  if (colon_idx == StringView::npos) {
    return length;
  }

  const auto key = trim(sv.substr(0, colon_idx));
  const auto value = trim(sv.substr(colon_idx + 1));

  request->response_headers_lower.emplace(to_lower(key), value);
  return length;
}

std::size_t CurlImpl::on_read_body(char *data, std::size_t, std::size_t length,
                                   void *user_data) {
  const auto request = static_cast<Request *>(user_data);
  request->response_body.append(data, length);
  return length;
}

CURLcode CurlImpl::log_on_error(CURLcode result) {
  if (result != CURLE_OK) {
    logger_->log_error(
        Error{Error::CURL_HTTP_CLIENT_ERROR, curl_.easy_strerror(result)});
  }
  return result;
}

CURLMcode CurlImpl::log_on_error(CURLMcode result) {
  if (result != CURLM_OK) {
    logger_->log_error(
        Error{Error::CURL_HTTP_CLIENT_ERROR, curl_.multi_strerror(result)});
  }
  return result;
}

void CurlImpl::run() {
  int num_messages_remaining;
  CURLMsg *message;
  constexpr int max_wait_milliseconds = 10000;
  std::unique_lock<std::mutex> lock(mutex_);

  for (;;) {
    log_on_error(curl_.multi_perform(multi_handle_, &num_active_handles_));
    if (num_active_handles_ == 0) {
      no_requests_.notify_all();
    }

    // If a request is done or errored out, curl will enqueue a "message" for
    // us to handle.  Handle any pending messages.
    while ((message = curl_.multi_info_read(multi_handle_,
                                            &num_messages_remaining))) {
      handle_message(*message, lock);
    }
    lock.unlock();
    log_on_error(curl_.multi_poll(multi_handle_, nullptr, 0,
                                  max_wait_milliseconds, nullptr));
    lock.lock();

    // New requests might have been added while we were sleeping.
    for (; !new_handles_.empty(); new_handles_.pop_front()) {
      CURL *handle = new_handles_.front();
      char *user_data;
      if (log_on_error(curl_.easy_getinfo_private(handle, &user_data)) !=
          CURLE_OK) {
        curl_.easy_cleanup(handle);
        continue;
      }

      auto *request = reinterpret_cast<Request *>(user_data);
      const auto timeout = request->deadline - clock_().tick;
      if (timeout <= std::chrono::steady_clock::time_point::duration::zero()) {
        std::string error_message;
        error_message +=
            "Request deadline exceeded before request was even added to "
            "libcurl "
            "event loop. Deadline was ";
        error_message += std::to_string(
            -std::chrono::duration_cast<std::chrono::nanoseconds>(timeout)
                 .count());
        error_message += " nanoseconds ago.";
        request->on_error(
            Error{Error::CURL_DEADLINE_EXCEEDED_BEFORE_REQUEST_START,
                  std::move(error_message)});

        curl_.easy_cleanup(handle);
        delete request;

        continue;
      }

      log_on_error(curl_.easy_setopt_timeout_ms(
          handle,
          static_cast<long>(
              std::chrono::duration_cast<std::chrono::milliseconds>(timeout)
                  .count())));
      log_on_error(curl_.multi_add_handle(multi_handle_, handle));
      request_handles_.insert(handle);
    }

    if (shutting_down_) {
      break;
    }
  }

  // We're shutting down.  Clean up any remaining request handles.
  for (const auto &handle : request_handles_) {
    char *user_data;
    if (log_on_error(curl_.easy_getinfo_private(handle, &user_data)) ==
        CURLE_OK) {
      delete reinterpret_cast<Request *>(user_data);
    }

    log_on_error(curl_.multi_remove_handle(multi_handle_, handle));
    curl_.easy_cleanup(handle);
  }

  request_handles_.clear();
}

void CurlImpl::handle_message(const CURLMsg &message,
                              std::unique_lock<std::mutex> &lock) {
  if (message.msg != CURLMSG_DONE) {
    return;
  }

  auto *const request_handle = message.easy_handle;
  char *user_data;
  if (log_on_error(curl_.easy_getinfo_private(request_handle, &user_data)) !=
      CURLE_OK) {
    return;
  }
  auto &request = *reinterpret_cast<Request *>(user_data);

  // `request` is done.  If we got a response, then call the response
  // handler.  If an error occurred, then call the error handler.
  const auto result = message.data.result;
  if (result != CURLE_OK) {
    std::string error_message;
    error_message += "Error sending request with libcurl (";
    error_message += curl_.easy_strerror(result);
    error_message += "): ";
    error_message += request.error_buffer;
    lock.unlock();
    request.on_error(
        Error{Error::CURL_REQUEST_FAILURE, std::move(error_message)});
    lock.lock();
  } else {
    long status;
    if (log_on_error(curl_.easy_getinfo_response_code(request_handle,
                                                      &status)) != CURLE_OK) {
      status = -1;
    }
    HeaderReader reader(&request.response_headers_lower);
    lock.unlock();
    request.on_response(static_cast<int>(status), reader,
                        std::move(request.response_body));
    lock.lock();
  }

  log_on_error(curl_.multi_remove_handle(multi_handle_, request_handle));
  curl_.easy_cleanup(request_handle);
  request_handles_.erase(request_handle);
  delete &request;
}

CurlImpl::Request::~Request() { curl->slist_free_all(request_headers); }

CurlImpl::HeaderWriter::HeaderWriter(CurlLibrary &curl) : curl_(curl) {}

CurlImpl::HeaderWriter::~HeaderWriter() { curl_.slist_free_all(list_); }

curl_slist *CurlImpl::HeaderWriter::release() {
  auto list = list_;
  list_ = nullptr;
  return list;
}

void CurlImpl::HeaderWriter::set(StringView key, StringView value) {
  buffer_.clear();
  buffer_ += key;
  buffer_ += ": ";
  buffer_ += value;

  list_ = curl_.slist_append(list_, buffer_.c_str());
}

CurlImpl::HeaderReader::HeaderReader(
    std::unordered_map<std::string, std::string> *response_headers_lower)
    : response_headers_lower_(response_headers_lower) {}

Optional<StringView> CurlImpl::HeaderReader::lookup(StringView key) const {
  buffer_ = to_lower(key);

  const auto found = response_headers_lower_->find(buffer_);
  if (found == response_headers_lower_->end()) {
    return nullopt;
  }
  return found->second;
}

void CurlImpl::HeaderReader::visit(
    const std::function<void(StringView key, StringView value)> &visitor)
    const {
  for (const auto &[key, value] : *response_headers_lower_) {
    visitor(key, value);
  }
}

}  // namespace tracing
}  // namespace datadog
