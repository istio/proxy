#pragma once

#include "envoy/server/filter_config.h"
#include "envoy/stream_info/filter_state.h"
#include "extensions/stats/config.pb.h"
#include "extensions/stats/config.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace IstioStats {

class IstioStatsFilterConfigFactory
    : public Common::FactoryBase<stats::PluginConfig> {
 public:
  IstioStatsFilterConfigFactory()
      : FactoryBase("envoy.filters.http.istio_stats") {}

 private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const stats::PluginConfig& proto_config, const std::string&,
      Server::Configuration::FactoryContext&) override;
};

}  // namespace IstioStats
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
