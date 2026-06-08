#include <datadog/baggage.h>
#include <datadog/dict_reader.h>
#include <datadog/string_view.h>

#include <cstdint>

namespace dd = datadog::tracing;

class MapReader : public dd::DictReader {
  std::unordered_map<std::string, std::string> map_;

 public:
  ~MapReader() override = default;

  MapReader(std::unordered_map<std::string, std::string> map)
      : map_(std::move(map)) {}

  dd::Optional<dd::StringView> lookup(dd::StringView key) const override {
    auto it = map_.find(std::string(key));
    if (it == map_.cend()) return dd::nullopt;

    return it->second;
  }

  void visit(const std::function<void(dd::StringView key,
                                      dd::StringView value)>&) const override{};
};

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, size_t size) {
  MapReader reader({{"baggage", std::string((const char*)data, size)}});
  dd::Baggage::extract(reader);
  return 0;
}
