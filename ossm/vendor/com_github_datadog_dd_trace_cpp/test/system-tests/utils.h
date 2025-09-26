#pragma once

#include <datadog/dict_reader.h>
#include <datadog/dict_writer.h>

#include <datadog/json.hpp>
#include <string>

namespace utils {

namespace dd = datadog::tracing;

inline std::string tolower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

template <typename T>
std::optional<T> get_if_exists(const nlohmann::json& j, std::string_view key) {
  if (auto it = j.find(key); it != j.cend()) {
    return it->get<T>();
  }
  return std::nullopt;
}

class HeaderReader final : public datadog::tracing::DictReader {
  const nlohmann::json::array_t headers_;

 public:
  HeaderReader(const nlohmann::json::array_t& headers) : headers_(headers) {}
  ~HeaderReader() override = default;

  // Return the value at the specified `key`, or return `nullopt` if there
  // is no value at `key`.
  dd::Optional<dd::StringView> lookup(dd::StringView key) const override {
    for (const auto& subarray : headers_) {
      if (subarray.size() == 2) {
        if (tolower(subarray[0].get<std::string>()) == std::string(key)) {
          return subarray[1].get<std::string_view>();
        } else if (tolower(subarray[1].get<std::string>()) ==
                   std::string(key)) {
          return subarray[0].get<std::string_view>();
        }
      }
    }

    return dd::nullopt;
  };

  // Invoke the specified `visitor` once for each key/value pair in this object.
  void visit(const std::function<void(dd::StringView key,
                                      dd::StringView value)>& /* visitor */)
      const override {}
};

class HeaderWriter final : public dd::DictWriter {
  nlohmann::json& j_;

 public:
  HeaderWriter(nlohmann::json& headers) : j_(headers){};
  ~HeaderWriter() = default;

  // Associate the specified `value` with the specified `key`.  An
  // implementation may, but is not required to, overwrite any previous value at
  // `key`.
  void set(dd::StringView key, dd::StringView value) override {
    j_.emplace_back(nlohmann::json::array({key, value}));
  };
};

}  // namespace utils
