#pragma once

#include <datadog/dict_reader.h>

#include <string>
#include <unordered_map>

using namespace datadog::tracing;

class MockDictReader : public DictReader {
  const std::unordered_map<std::string, std::string>* map_;

 public:
  MockDictReader() : map_(nullptr){};
  explicit MockDictReader(
      const std::unordered_map<std::string, std::string>& map)
      : map_(&map) {}

  Optional<StringView> lookup(StringView key) const override {
    if (map_ == nullptr) return nullopt;

    auto found = map_->find(std::string(key));
    if (found == map_->end()) {
      return nullopt;
    }
    return found->second;
  }

  void visit(const std::function<void(StringView key, StringView value)>&
                 visitor) const override {
    if (map_ == nullptr) return;

    for (const auto& [key, value] : *map_) {
      visitor(key, value);
    }
  }
};
