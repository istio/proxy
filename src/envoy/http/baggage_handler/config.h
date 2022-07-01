#pragma once

#include "source/extensions/filters/http/common/factory_base.h"
#include "src/envoy/http/baggage_handler/config/baggage_handler.pb.h"

namespace Envoy {
namespace Http {
namespace BaggageHandler {

/**
 * Config registration for the baggage handler filter.
 */
class BaggageHandlerConfigFactory
    : public Server::Configuration::NamedHttpFilterConfigFactory {
 public:
  // Server::Configuration::NamedHttpFilterConfigFactory
  Http::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message &config, const std::string &stat_prefix,
      Server::Configuration::FactoryContext &context) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() const override;

 private:
  Http::FilterFactoryCb createFilterFactory(
      const istio::telemetry::baggagehandler::v1::Config &config_pb);
};

}  // namespace BaggageHandler
}  // namespace Http
}  // namespace Envoy
