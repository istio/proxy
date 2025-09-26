#pragma once

#include <datadog/dict_writer.h>
#include <datadog/string_view.h>

#include <string>
#include <unordered_map>

using namespace datadog::tracing;

struct MockDictWriter : public DictWriter {
  std::unordered_map<std::string, std::string> items;

  void set(StringView key, StringView value) override {
    items.insert_or_assign(std::string(key), std::string(value));
  }
};
