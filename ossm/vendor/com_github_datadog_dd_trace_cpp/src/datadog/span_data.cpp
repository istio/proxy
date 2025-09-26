#include "span_data.h"

#include <cassert>
#include <cstddef>

#include "error.h"
#include "msgpack.h"
#include "span_config.h"
#include "span_defaults.h"
#include "string_view.h"
#include "tags.h"

namespace datadog {
namespace tracing {
namespace {

Optional<StringView> lookup(
    const std::string& key,
    const std::unordered_map<std::string, std::string>& map) {
  const auto found = map.find(key);
  if (found != map.end()) {
    return found->second;
  }
  return nullopt;
}

}  // namespace

Optional<StringView> SpanData::environment() const {
  return lookup(tags::environment, tags);
}

Optional<StringView> SpanData::version() const {
  return lookup(tags::version, tags);
}

void SpanData::apply_config(const SpanDefaults& defaults,
                            const SpanConfig& config, const Clock& clock) {
  service = config.service.value_or(defaults.service);
  name = config.name.value_or(defaults.name);

  for (const auto& item : defaults.tags) {
    tags.insert(item);
  }
  std::string environment = config.environment.value_or(defaults.environment);
  if (!environment.empty()) {
    tags.insert_or_assign(tags::environment, environment);
  }
  std::string version = config.version.value_or(defaults.version);
  if (!version.empty()) {
    tags.insert_or_assign(tags::version, version);
  }
  for (const auto& [key, value] : config.tags) {
    tags.insert_or_assign(key, value);
  }

  resource = config.resource.value_or(name);
  service_type = config.service_type.value_or(defaults.service_type);
  if (config.start) {
    start = *config.start;
  } else {
    start = clock();
  }
}

Expected<void> msgpack_encode(std::string& destination, const SpanData& span) {
  // clang-format off
  msgpack::pack_map(
      destination,
      "service", [&](auto& destination) {
         return msgpack::pack_string(destination, span.service);
       },
      "name", [&](auto& destination) {
         return msgpack::pack_string(destination, span.name);
       },
      "resource", [&](auto& destination) {
         return msgpack::pack_string(destination, span.resource);
       },
      "trace_id", [&](auto& destination) {
         msgpack::pack_integer(destination, span.trace_id.low);
         return Expected<void>{};
       },
      "span_id", [&](auto& destination) {
         msgpack::pack_integer(destination, span.span_id);
         return Expected<void>{};
       },
      "parent_id", [&](auto& destination) {
         msgpack::pack_integer(destination, span.parent_id);
         return Expected<void>{};
       },
      "start", [&](auto& destination) {
         msgpack::pack_integer(
             destination, std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                              span.start.wall.time_since_epoch())
                              .count()));
         return Expected<void>{};
       },
      "duration", [&](auto& destination) {
         msgpack::pack_integer(
             destination,
             std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(span.duration)
                 .count()));
        return Expected<void>{};
       },
      "error", [&](auto& destination) {
         msgpack::pack_integer(destination, std::int32_t(span.error));
         return Expected<void>{};
       },
      "meta", [&](auto& destination) {
         return msgpack::pack_map(destination, span.tags,
                           [](std::string& destination, const auto& value) {
                             return msgpack::pack_string(destination, value);
                           });
       }, "metrics",
       [&](auto& destination) {
         return msgpack::pack_map(destination, span.numeric_tags,
                           [](std::string& destination, const auto& value) {
                             msgpack::pack_double(destination, value);
                             return Expected<void>{};
                           });
       }, "type", [&](auto& destination) {
         return msgpack::pack_string(destination, span.service_type);
       });
  // clang-format on

  return nullopt;
}

Expected<void> msgpack_encode(
    std::string& destination,
    const std::vector<std::unique_ptr<SpanData>>& spans) {
  return msgpack::pack_array(destination, spans,
                             [](auto& destination, const auto& span_ptr) {
                               assert(span_ptr);
                               return msgpack_encode(destination, *span_ptr);
                             });
}

}  // namespace tracing
}  // namespace datadog
