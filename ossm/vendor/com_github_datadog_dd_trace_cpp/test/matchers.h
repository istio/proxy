#pragma once

#include <algorithm>
#include <sstream>
#include <vector>

#include "test.h"

template <typename Map>
class ContainsSubset : public Catch::MatcherBase<Map> {
  const Map* subset_;

  // `find` for when we're comparing with a `vector`.
  template <typename Key, typename Value, typename LookupKey>
  static auto find(const std::vector<Key, Value>& other, const LookupKey& key) {
    return std::find_if(other.begin(), other.end(),
                        [&](const auto& entry) { return entry.first == key; });
  }

  // `find` for when we're comparing with an associative container.
  template <typename Other, typename LookupKey>
  static auto find(const Other& other, const LookupKey& key) {
    return other.find(key);
  }

 public:
  ContainsSubset(const Map& subset) : subset_(&subset) {}

  bool match(const Map& other) const override {
    return std::all_of(subset_->begin(), subset_->end(), [&](const auto& item) {
      const auto& [key, value] = item;
      auto found = find(other, key);
      return found != other.end() && found->second == value;
    });
  }

  std::string describe() const override {
    std::ostringstream stream;
    stream << "ContainsSubset: {";
    for (const auto& [key, value] : *subset_) {
      stream << "  {";
      stream << key;
      stream << ", ";
      stream << value;
      stream << "}";
    }
    stream << "  }";
    return stream.str();
  }
};
