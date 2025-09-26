#include "datadog/dict_reader.h"
#include "datadog/dict_writer.h"
#include "httplib.h"

namespace tracingutil {
// `HeaderWriter` and `HeaderReader` adapt dd-trace-cpp's writer and reader
// interfaces, respectively, to the HTTP headers object used by this app's HTTP
// library.

// dd-trace-cpp uses `HeaderWriter` to inject trace context into outgoing HTTP
// request headers.
class HeaderWriter final : public datadog::tracing::DictWriter {
  httplib::Headers& headers_;

 public:
  explicit HeaderWriter(httplib::Headers& headers) : headers_(headers) {}

  void set(std::string_view key, std::string_view value) override {
    auto found = headers_.find(std::string(key));
    if (found == headers_.cend()) {
      headers_.emplace(key, value);
    } else {
      found->second = value;
    }
  }
};

// dd-trace-cpp uses `HeaderReader` to extract trace context from incoming HTTP
// request headers.
class HeaderReader : public datadog::tracing::DictReader {
  const httplib::Headers& headers_;
  mutable std::string buffer_;

 public:
  explicit HeaderReader(const httplib::Headers& headers) : headers_(headers) {}

  void visit(
      const std::function<void(std::string_view key, std::string_view value)>&
          visitor) const override {
    for (const auto& [key, value] : headers_) {
      visitor(key, value);
    }
  }

  std::optional<std::string_view> lookup(std::string_view key) const override {
    // If there's no matching header, then return `std::nullopt`.
    // If there is one matching header, then return a view of its value.
    // If there are multiple matching headers, then join their values with
    // commas and return a view of the result.
    const auto [begin, end] = headers_.equal_range(std::string{key});
    switch (std::distance(begin, end)) {
      case 0:
        return std::nullopt;
      case 1:
        return begin->second;
    }
    auto it = begin;
    buffer_ = it->second;
    ++it;
    do {
      buffer_ += ',';
      buffer_ += it->second;
      ++it;
    } while (it != end);
    return buffer_;
  }
};
}  // namespace tracingutil
