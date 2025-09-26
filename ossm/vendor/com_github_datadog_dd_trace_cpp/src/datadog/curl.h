#pragma once

// This component provides a `class`, `Curl`, that implements the `HTTPClient`
// interface in terms of [libcurl][1].  `class Curl` manages a thread that is
// used as the event loop for libcurl.
//
// If this library was built in a mode that does not include libcurl, then this
// file and its implementation, `curl.cpp`, will not be included.
//
// [1]: https://curl.se/libcurl/

#include <curl/curl.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "clock.h"
#include "http_client.h"
#include "json_fwd.hpp"

namespace datadog {
namespace tracing {

// `class CurlLibrary` has one member function for every libcurl function used
// in the implementation of this component.
//
// The naming convention is that `CurlLibrary::foo_bar` corresponds to
// `curl_foo_bar`, with the exception of `curl_easy_getinfo` and
// `curl_easy_setopt`. `curl_easy_getinfo` and `curl_easy_setopt` have multiple
// corresponding member functions -- one for each `CURLINFO` value or
// `CURLoption` value, respectively.
//
// The default implementations forward to their libcurl counterparts.  Unit
// tests override some of the member functions.
class CurlLibrary {
 public:
  typedef size_t (*WriteCallback)(char *ptr, size_t size, size_t nmemb,
                                  void *userdata);
  typedef size_t (*HeaderCallback)(char *buffer, size_t size, size_t nitems,
                                   void *userdata);

  virtual ~CurlLibrary() = default;

  virtual void easy_cleanup(CURL *handle);
  virtual CURL *easy_init();
  virtual CURLcode easy_getinfo_private(CURL *curl, char **user_data);
  virtual CURLcode easy_getinfo_response_code(CURL *curl, long *code);
  virtual CURLcode easy_setopt_errorbuffer(CURL *handle, char *buffer);
  virtual CURLcode easy_setopt_headerdata(CURL *handle, void *data);
  virtual CURLcode easy_setopt_headerfunction(CURL *handle, HeaderCallback);
  virtual CURLcode easy_setopt_httpheader(CURL *handle, curl_slist *headers);
  virtual CURLcode easy_setopt_post(CURL *handle, long post);
  virtual CURLcode easy_setopt_postfields(CURL *handle, const char *data);
  virtual CURLcode easy_setopt_postfieldsize(CURL *handle, long size);
  virtual CURLcode easy_setopt_private(CURL *handle, void *pointer);
  virtual CURLcode easy_setopt_unix_socket_path(CURL *handle, const char *path);
  virtual CURLcode easy_setopt_url(CURL *handle, const char *url);
  virtual CURLcode easy_setopt_writedata(CURL *handle, void *data);
  virtual CURLcode easy_setopt_writefunction(CURL *handle, WriteCallback);
  virtual CURLcode easy_setopt_timeout_ms(CURL *handle, long timeout_ms);
  virtual const char *easy_strerror(CURLcode error);
  virtual void global_cleanup();
  virtual CURLcode global_init(long flags);
  virtual CURLMcode multi_add_handle(CURLM *multi_handle, CURL *easy_handle);
  virtual CURLMcode multi_cleanup(CURLM *multi_handle);
  virtual CURLMsg *multi_info_read(CURLM *multi_handle, int *msgs_in_queue);
  virtual CURLM *multi_init();
  virtual CURLMcode multi_perform(CURLM *multi_handle, int *running_handles);
  virtual CURLMcode multi_poll(CURLM *multi_handle, curl_waitfd extra_fds[],
                               unsigned extra_nfds, int timeout_ms,
                               int *numfds);
  virtual CURLMcode multi_remove_handle(CURLM *multi_handle, CURL *easy_handle);
  virtual const char *multi_strerror(CURLMcode error);
  virtual CURLMcode multi_wakeup(CURLM *multi_handle);
  virtual curl_slist *slist_append(curl_slist *list, const char *string);
  virtual void slist_free_all(curl_slist *list);
};

class CurlImpl;
class Logger;

class Curl : public HTTPClient {
  CurlImpl *impl_;

 public:
  using ThreadGenerator = std::function<std::thread(std::function<void()> &&)>;

  explicit Curl(const std::shared_ptr<Logger> &, const Clock &);
  Curl(const std::shared_ptr<Logger> &, const Clock &, CurlLibrary &);
  Curl(const std::shared_ptr<Logger> &, const Clock &, CurlLibrary &,
       const ThreadGenerator &);
  ~Curl();

  Curl(const Curl &) = delete;

  Expected<void> post(const URL &url, HeadersSetter set_headers,
                      std::string body, ResponseHandler on_response,
                      ErrorHandler on_error,
                      std::chrono::steady_clock::time_point deadline) override;

  void drain(std::chrono::steady_clock::time_point deadline) override;

  nlohmann::json config_json() const override;
};

}  // namespace tracing
}  // namespace datadog
