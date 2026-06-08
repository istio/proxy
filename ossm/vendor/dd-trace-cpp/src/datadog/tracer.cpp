#include <datadog/dict_reader.h>
#include <datadog/environment.h>
#include <datadog/id_generator.h>
#include <datadog/logger.h>
#include <datadog/runtime_id.h>
#include <datadog/span.h>
#include <datadog/span_config.h>
#include <datadog/telemetry/telemetry.h>
#include <datadog/trace_segment.h>
#include <datadog/tracer.h>
#include <datadog/tracer_signature.h>
#include <datadog/version.h>

#include <algorithm>
#include <cassert>

#include "config_manager.h"
#include "datadog_agent.h"
#include "extracted_data.h"
#include "extraction_util.h"
#include "hex.h"
#include "json.hpp"
#include "msgpack.h"
#include "platform_util.h"
#include "random.h"
#include "span_data.h"
#include "span_sampler.h"
#include "tags.h"
#include "telemetry_metrics.h"
#include "trace_sampler.h"
#include "w3c_propagation.h"

namespace datadog {
namespace tracing {

void to_json(nlohmann::json& j, const PropagationStyle& style) {
  j = to_string_view(style);
}

Tracer::Tracer(const FinalizedTracerConfig& config)
    : Tracer(config, default_id_generator(config.generate_128bit_trace_ids)) {}

Tracer::Tracer(const FinalizedTracerConfig& config,
               const std::shared_ptr<const IDGenerator>& generator)
    : logger_(config.logger),
      runtime_id_(config.runtime_id ? *config.runtime_id
                                    : RuntimeID::generate()),
      signature_{runtime_id_, config.defaults.service,
                 config.defaults.environment},
      config_manager_(std::make_shared<ConfigManager>(config)),
      collector_(/* see constructor body */),
      span_sampler_(
          std::make_shared<SpanSampler>(config.span_sampler, config.clock)),
      generator_(generator),
      clock_(config.clock),
      injection_styles_(config.injection_styles),
      extraction_styles_(config.extraction_styles),
      tags_header_max_size_(config.tags_header_size),
      baggage_opts_(config.baggage_opts),
      baggage_injection_enabled_(false),
      baggage_extraction_enabled_(false),
      tracing_enabled_(config.tracing_enabled) {
  telemetry::init(config.telemetry, signature_, logger_, config.http_client,
                  config.event_scheduler, config.agent_url);
  if (config.report_hostname) {
    hostname_ = get_hostname();
  }
  if (auto* collector =
          std::get_if<std::shared_ptr<Collector>>(&config.collector)) {
    collector_ = *collector;
  } else {
    auto& agent_config =
        std::get<FinalizedDatadogAgentConfig>(config.collector);

    auto rc_listeners = agent_config.remote_configuration_listeners;
    rc_listeners.emplace_back(config_manager_);
    auto agent = std::make_shared<DatadogAgent>(agent_config, config.logger,
                                                signature_, rc_listeners);
    collector_ = agent;
  }

  for (const auto style : extraction_styles_) {
    if (style == PropagationStyle::BAGGAGE) {
      baggage_extraction_enabled_ = true;
      break;
    }
  }

  for (const auto style : injection_styles_) {
    if (style == PropagationStyle::BAGGAGE) {
      baggage_injection_enabled_ = true;
      break;
    }
  }

  if (config.log_on_startup) {
    logger_->log_startup([configuration = this->config()](std::ostream& log) {
      log << "DATADOG TRACER CONFIGURATION - " << configuration;
    });
  }

  store_config();
}

std::string Tracer::config() const {
  // clang-format off
  auto config = nlohmann::json::object({
    {"version", tracer_version_string},
    {"runtime_id", runtime_id_.string()},
    {"collector", nlohmann::json::parse(collector_->config())},
    {"span_sampler", span_sampler_->config_json()},
    {"injection_styles", injection_styles_},
    {"extraction_styles", extraction_styles_},
    {"tags_header_size", tags_header_max_size_},
    {"environment_variables", nlohmann::json::parse(environment::to_json())},
    {"baggage", nlohmann::json{
      {"max_bytes", baggage_opts_.max_bytes},
      {"max_items", baggage_opts_.max_items},
    }},
  });
  // clang-format on

  config.merge_patch(config_manager_->config_json());

  if (hostname_) {
    config["hostname"] = *hostname_;
  }

  return config.dump();
}

void Tracer::store_config() {
  auto maybe_file =
      InMemoryFile::make(std::string("datadog-tracer-info-") + short_uuid());
  if (auto error = maybe_file.if_error()) {
    if (error->code == Error::Code::NOT_IMPLEMENTED) return;

    logger_->log_error("Failed to open anonymous file");
    return;
  }

  metadata_file_ = std::make_unique<InMemoryFile>(std::move(*maybe_file));

  auto defaults = config_manager_->span_defaults();

  std::string buffer;
  buffer.reserve(1024);

  // clang-format off
  msgpack::pack_map(
    buffer, 
    "schema_version", [&](auto& buffer) { msgpack::pack_integer(buffer, std::uint64_t(1)); return Expected<void>{}; },
    "runtime_id", [&](auto& buffer) { return msgpack::pack_string(buffer, runtime_id_.string()); },
    "tracer_version", [&](auto& buffer) { return msgpack::pack_string(buffer, signature_.library_version); },
    "tracer_language", [&](auto& buffer) { return msgpack::pack_string(buffer, signature_.library_language); },
    "hostname", [&](auto& buffer) { return msgpack::pack_string(buffer, hostname_.value_or("")); },
    "service_name", [&](auto& buffer) { return msgpack::pack_string(buffer, defaults->service); },
    "service_env", [&](auto& buffer) { return msgpack::pack_string(buffer, defaults->environment); },
    "service_version", [&](auto& buffer) { return msgpack::pack_string(buffer, defaults->version); }
  );
  // clang-format on

  if (!metadata_file_->write_then_seal(buffer)) {
    logger_->log_error("Either failed to write or seal the configuration file");
  }
}

Span Tracer::create_span() { return create_span(SpanConfig{}); }

Span Tracer::create_span(const SpanConfig& config) {
  auto defaults = config_manager_->span_defaults();
  auto span_data = std::make_unique<SpanData>();
  span_data->apply_config(*defaults, config, clock_);
  span_data->trace_id = generator_->trace_id(span_data->start);
  span_data->span_id = span_data->trace_id.low;
  span_data->parent_id = 0;

  std::vector<std::pair<std::string, std::string>> trace_tags;
  if (span_data->trace_id.high) {
    trace_tags.emplace_back(tags::internal::trace_id_high,
                            hex_padded(span_data->trace_id.high));
  }

  const auto span_data_ptr = span_data.get();
  telemetry::counter::increment(metrics::tracer::trace_segments_created,
                                {"new_continued:new"});
  const auto segment = std::make_shared<TraceSegment>(
      logger_, collector_, config_manager_->trace_sampler(), span_sampler_,
      defaults, config_manager_, runtime_id_, injection_styles_, hostname_,
      nullopt /* origin */, tags_header_max_size_, std::move(trace_tags),
      nullopt /* sampling_decision */, nullopt /* additional_w3c_tracestate */,
      nullopt /* additional_datadog_w3c_tracestate*/, std::move(span_data),
      tracing_enabled_);
  Span span{span_data_ptr, segment,
            [generator = generator_]() { return generator->span_id(); },
            clock_};
  return span;
}

Expected<Span> Tracer::extract_span(const DictReader& reader) {
  return extract_span(reader, SpanConfig{});
}

Expected<Span> Tracer::extract_span(const DictReader& reader,
                                    const SpanConfig& config) {
  assert(!extraction_styles_.empty());

  AuditedReader audited_reader{reader};

  auto span_data = std::make_unique<SpanData>();
  Optional<PropagationStyle> first_style_with_trace_id;
  Optional<PropagationStyle> first_style_with_parent_id;
  std::unordered_map<PropagationStyle, ExtractedData> extracted_contexts;

  for (const auto style : extraction_styles_) {
    using Extractor = decltype(&extract_datadog);  // function pointer
    Extractor extract;
    std::string extracted_tag;  ///< for telemetry
    switch (style) {
      case PropagationStyle::DATADOG:
        extract = &extract_datadog;
        extracted_tag = "header_style:datadog";
        break;
      case PropagationStyle::B3:
        extract = &extract_b3;
        extracted_tag = "header_style:b3multi";
        break;
      case PropagationStyle::W3C:
        extract = &extract_w3c;
        extracted_tag = "header_style:tracecontext";
        break;
      default:
        extract = &extract_none;
        extracted_tag = "header_style:none";
    }
    audited_reader.entries_found.clear();
    auto data = extract(audited_reader, span_data->tags, *logger_);
    if (auto* error = data.if_error()) {
      return error->with_prefix(
          extraction_error_prefix(style, audited_reader.entries_found));
    }

    telemetry::counter::increment(metrics::tracer::trace_context::extracted,
                                  {extracted_tag});

    if (!first_style_with_trace_id && data->trace_id.has_value()) {
      first_style_with_trace_id = style;
    }

    if (!first_style_with_parent_id && data->parent_id.has_value()) {
      first_style_with_parent_id = style;
    }

    data->headers_examined = audited_reader.entries_found;
    extracted_contexts.emplace(style, std::move(*data));
  }

  ExtractedData merged_context;
  if (!first_style_with_trace_id) {
    // Nothing extracted a trace ID. Return the first context that includes a
    // parent ID, if any, or otherwise just return an empty `ExtractedData`.
    // The purpose of looking for a parent ID is to allow for the error
    // "extracted a parent ID without a trace ID," if that's what happened.
    if (first_style_with_parent_id) {
      auto other = extracted_contexts.find(*first_style_with_parent_id);
      assert(other != extracted_contexts.end());
      merged_context = other->second;
    }
  } else {
    merged_context = merge(*first_style_with_trace_id, extracted_contexts);
  }

  // Some information might be missing.
  // Here are the combinations considered:
  //
  // - no trace ID and no parent ID
  //     - this means there's no span to extract
  // - parent ID and no trace ID
  //     - error
  // - trace ID and no parent ID
  //     - if origin is set, then we're extracting a root span
  //         - the idea is that "synthetics" might have started a trace without
  //           producing a root span
  //     - if origin is _not_ set, then it's an error
  // - trace ID and parent ID means we're extracting a child span
  // - if trace ID is zero, then that's an error.

  if (!merged_context.trace_id && !merged_context.parent_id) {
    return Error{Error::NO_SPAN_TO_EXTRACT,
                 "There's neither a trace ID nor a parent span ID to extract."}
        .with_prefix(extraction_error_prefix(merged_context.style,
                                             merged_context.headers_examined));
  }
  if (!merged_context.trace_id) {
    std::string message;
    message +=
        "There's no trace ID to extract, but there is a parent span ID: ";
    message += std::to_string(*merged_context.parent_id);
    return Error{Error::MISSING_TRACE_ID, std::move(message)}.with_prefix(
        extraction_error_prefix(merged_context.style,
                                merged_context.headers_examined));
  }
  if (!merged_context.parent_id && !merged_context.origin) {
    std::string message;
    message +=
        "There's no parent span ID to extract, but there is a trace ID: ";
    message += "[hexadecimal = ";
    message += merged_context.trace_id->hex_padded();
    if (merged_context.trace_id->high == 0) {
      message += ", decimal = ";
      message += std::to_string(merged_context.trace_id->low);
    }
    message += ']';
    return Error{Error::MISSING_PARENT_SPAN_ID, std::move(message)}.with_prefix(
        extraction_error_prefix(merged_context.style,
                                merged_context.headers_examined));
  }

  if (!merged_context.parent_id) {
    // We have a trace ID, but not parent ID.  We're meant to be the root, and
    // whoever called us already created a trace ID for us (to correlate with
    // whatever they're doing).
    merged_context.parent_id = 0;
  }

  assert(merged_context.parent_id);
  assert(merged_context.trace_id);

  if (*merged_context.trace_id == 0) {
    return Error{Error::ZERO_TRACE_ID,
                 "extracted zero value for trace ID, which is invalid"}
        .with_prefix(extraction_error_prefix(merged_context.style,
                                             merged_context.headers_examined));
  }

  // We're done extracting fields.  Now create the span.
  // This is similar to what we do in `create_span`.
  span_data->apply_config(*config_manager_->span_defaults(), config, clock_);
  span_data->span_id = generator_->span_id();
  span_data->trace_id = *merged_context.trace_id;
  span_data->parent_id = *merged_context.parent_id;

  if (span_data->trace_id.high) {
    // The trace ID has some bits set in the higher 64 bits. Set the
    // corresponding `trace_id_high` tag, so that the Datadog backend is aware
    // of those bits.
    //
    // First, though, if the `trace_id_high` tag is already set and has a
    // bogus value or a value inconsistent with the trace ID, tag an error.
    const auto hex_high = hex_padded(span_data->trace_id.high);
    const auto extant =
        std::find_if(merged_context.trace_tags.begin(),
                     merged_context.trace_tags.end(), [&](const auto& pair) {
                       return pair.first == tags::internal::trace_id_high;
                     });
    if (extant == merged_context.trace_tags.end()) {
      merged_context.trace_tags.emplace_back(tags::internal::trace_id_high,
                                             hex_high);
    } else {
      // There is already a `trace_id_high` tag. `hex_high` is its proper
      // value. Check if the extant value is malformed or different from
      // `hex_high`. In either case, tag an error and overwrite the tag with
      // `hex_high`.
      const Optional<std::uint64_t> high = parse_trace_id_high(extant->second);
      if (!high) {
        span_data->tags[tags::internal::propagation_error] =
            "malformed_tid " + extant->second;
        extant->second = hex_high;
      } else if (*high != span_data->trace_id.high) {
        span_data->tags[tags::internal::propagation_error] =
            "inconsistent_tid " + extant->second;
        extant->second = hex_high;
      }
    }
  }

  if (merged_context.datadog_w3c_parent_id) {
    span_data->tags[tags::internal::w3c_parent_id] =
        *merged_context.datadog_w3c_parent_id;
  }

  // Trace source tag is not a trace tag, move it to a simple tag on the local
  // root span.
  if (const auto found = std::find_if(
          merged_context.trace_tags.cbegin(), merged_context.trace_tags.cend(),
          [](const auto& p) {
            return p.first == tags::internal::trace_source;
          });
      found != merged_context.trace_tags.cend()) {
    span_data->tags.emplace(tags::internal::trace_source, found->second);
    merged_context.trace_tags.erase(found);
  }

  // When APM Tracing is disabled, the incoming sampling decision MAY be
  // overridden based on locally generated spans. As such, the received sampling
  // decision is intentionally ignored, and the tracer is expected to make its
  // own decision in accordance with the locally enabled product configuration.
  Optional<SamplingDecision> sampling_decision;
  if (tracing_enabled_ && merged_context.sampling_priority) {
    SamplingDecision decision;
    decision.priority = *merged_context.sampling_priority;
    // `decision.mechanism` is null.  We might be able to infer it once we
    // extract `trace_tags`, but we would have no use for it, so we won't.
    decision.origin = SamplingDecision::Origin::EXTRACTED;

    sampling_decision = decision;
  }

  const auto span_data_ptr = span_data.get();
  telemetry::counter::increment(metrics::tracer::trace_segments_created,
                                {"new_continued:continued"});
  const auto segment = std::make_shared<TraceSegment>(
      logger_, collector_, config_manager_->trace_sampler(), span_sampler_,
      config_manager_->span_defaults(), config_manager_, runtime_id_,
      injection_styles_, hostname_, std::move(merged_context.origin),
      tags_header_max_size_, std::move(merged_context.trace_tags),
      std::move(sampling_decision),
      std::move(merged_context.additional_w3c_tracestate),
      std::move(merged_context.additional_datadog_w3c_tracestate),
      std::move(span_data), tracing_enabled_);
  Span span{span_data_ptr, segment,
            [generator = generator_]() { return generator->span_id(); },
            clock_};
  return span;
}

Span Tracer::extract_or_create_span(const DictReader& reader) {
  return extract_or_create_span(reader, SpanConfig{});
}

Span Tracer::extract_or_create_span(const DictReader& reader,
                                    const SpanConfig& config) {
  auto maybe_span = extract_span(reader, config);
  if (maybe_span) {
    return std::move(*maybe_span);
  }
  return create_span(config);
}

Baggage Tracer::create_baggage() { return Baggage(baggage_opts_.max_items); }

Expected<Baggage, Baggage::Error> Tracer::extract_baggage(
    const DictReader& reader) {
  if (!baggage_extraction_enabled_) {
    return Baggage::Error{Baggage::Error::DISABLED};
  }

  auto maybe_baggage = Baggage::extract(reader);
  if (maybe_baggage) {
    telemetry::counter::increment(metrics::tracer::trace_context::extracted,
                                  {"header_style:baggage"});
  } else if (auto err = maybe_baggage.if_error()) {
    if (err->code == Baggage::Error::MALFORMED_BAGGAGE_HEADER) {
      telemetry::counter::increment(metrics::tracer::trace_context::malformed,
                                    {"header_style:baggage"});
    }
  }
  return maybe_baggage;
}

Baggage Tracer::extract_or_create_baggage(const DictReader& reader) {
  auto maybe_baggage = extract_baggage(reader);
  if (maybe_baggage) {
    return std::move(*maybe_baggage);
  }

  return create_baggage();
}

Expected<void> Tracer::inject(const Baggage& baggage, DictWriter& writer) {
  if (!baggage_injection_enabled_) {
    // TODO(@dmehala): update `Expected` to support `<void, Error>`
    return Error{Error::Code::OTHER, "Baggage propagation is disabled"};
  }

  auto res = baggage.inject(writer, baggage_opts_);
  if (auto err = res.if_error()) {
    logger_->log_error(
        err->with_prefix("failed to serialize all baggage items: "));

    if (err->code == Error::Code::BAGGAGE_MAXIMUM_BYTES_REACHED) {
      telemetry::counter::increment(
          metrics::tracer::trace_context::truncated,
          {"truncation_reason:baggage_byte_count_exceeded"});
    } else if (err->code == Error::Code::BAGGAGE_MAXIMUM_ITEMS_REACHED) {
      telemetry::counter::increment(
          metrics::tracer::trace_context::truncated,
          {"truncation_reason:baggage_item_count_exceeded"});
    } else {
      telemetry::counter::increment(metrics::tracer::trace_context::injected,
                                    {"header_style:baggage"});
    }
  }

  return {};
}

}  // namespace tracing
}  // namespace datadog
