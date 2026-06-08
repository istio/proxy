#pragma once

#include <datadog/dict_reader.h>
#include <datadog/dict_writer.h>
#include <datadog/expected.h>
#include <datadog/optional.h>
#include <datadog/string_view.h>

#include <string>
#include <unordered_map>

namespace datadog {
namespace tracing {

/// OpenTelemetry-like implementation of the Baggage concept.
/// Baggage is a key-value store meant to propagate data across services and
/// processes boundaries.
///
/// Baggage are extracted from any tracing context implementing the `DictReader`
/// interface using `Baggage::extract`.
///
/// Baggages are injected to any tracing context implementing the `DictWriter`
/// interface using the `inject` method.
class Baggage {
 public:
  struct Error final {
    enum Code : char {
      /// Baggage propagation is disabled. This may be due to one of the
      /// following
      /// reasons:
      /// - `baggage` is not set as an extraction or injection propagation
      /// style.
      /// - The maximum number of items is less than 0.
      /// - The number of bytes is less than 3.
      DISABLED,
      MISSING_HEADER,
      MALFORMED_BAGGAGE_HEADER,
      MAXIMUM_CAPACITY_REACHED,
      MAXIMUM_BYTES_REACHED,
    };
    Code code;
    Optional<size_t> pos;

    Error(Code in_code) : code(in_code), pos(nullopt) {}
    Error(Code in_code, size_t position) : code(in_code), pos(position) {}
  };

  struct Options final {
    size_t max_bytes;
    size_t max_items;
  };

  static constexpr size_t default_max_capacity = 64;
  static constexpr Options default_options{2048, default_max_capacity};

  /// Extracts a Baggage instance from a `DictReader` and creates a Baggage
  /// instance if no errors are encounters .
  ///
  /// @param `reader` The input `DictReader` from which to extract the data.
  /// @return A `Baggage` instance or an `Error`.
  static Expected<Baggage, Error> extract(const DictReader& reader);

  /// Initializes an empty Baggage with the default maximum capacity.
  Baggage() = default;

  /// Initializes an empty Baggage instance with the given maximum capacity.
  ///
  /// @param `max_capacity` The maximum capacity for this Baggage instance.
  Baggage(size_t max_capacity);

  /// Initializes a Baggage instance using the provided unordered_map of
  /// key-value pairs. The maximum capacity can also be specified.
  ///
  /// @param `baggage_map` The map containing key-value pairs to initialize the
  /// Baggage.
  /// @param `max_capacity` The maximum capacity for this Baggage instance.
  Baggage(std::unordered_map<std::string, std::string>,
          size_t max_capacity = default_max_capacity);

  /// Checks if the Baggage contains a specified key.
  ///
  /// @param `key` The key to check.
  /// @return `true` if the key exists in the Baggage; otherwise, `false`.
  bool contains(StringView key) const;

  /// Retrieves the value associated with a specified key.
  ///
  /// @param `key` The key to retrieve the value for.
  /// @return An `Optional<StringView>` containing the value if the key exists,
  /// or an empty Optional if the key is not found.
  Optional<StringView> get(StringView key) const;

  /// Adds a key-value pair to the Baggage.
  ///
  /// This function will attempt to add the given key-value pair to the Baggage.
  /// If the maximum capacity has been reached, the insertion will fail.
  /// If a `key` already exists, its value will be overwritten with `value`.
  ///
  /// @param `key` The key to insert.
  /// @param `value` The value to associate with the key.
  /// @return `true` if the key-value pair was successfully added; `false` if
  /// the maximum capacity was reached.
  bool set(std::string key, std::string value);

  /// Removes the key-value pair corresponding to the specified key.
  ///
  /// @param `key` The key to remove from the Baggage.
  void remove(StringView key);

  /// Removes all key-value pair.
  void clear();

  /// Retrieves the number of items stored.
  size_t size() const;

  /// Returns whether any items are stored.
  bool empty() const;

  /// Visits each key-value pair in the Baggage and invoke the provided
  /// visitor function for each key-value pair in the Baggage.
  ///
  /// @param `visitor` A function object that will be called for each
  /// key-value pair.
  void visit(std::function<void(StringView, StringView)>&& visitor);

  /// Injects the Baggage data into a `DictWriter` with the constraint that
  /// the amount of bytes written does not exceed the specified maximum byte
  /// limit.
  ///
  /// @param `writer` The DictWriter to inject the data into.
  /// @param `opts` Injection options.
  /// @return An `Expected<void>`, which may either succeed or contain an
  /// error.
  Expected<void> inject(DictWriter& writer,
                        const Options& opts = default_options) const;

  /// Equality operator for comparing two Baggage instances.
  inline bool operator==(const Baggage& rhs) const {
    return baggage_ == rhs.baggage_;
  }

 private:
  const size_t max_capacity_ = Baggage::default_max_capacity;
  std::unordered_map<std::string, std::string> baggage_;
};

}  // namespace tracing
}  // namespace datadog
