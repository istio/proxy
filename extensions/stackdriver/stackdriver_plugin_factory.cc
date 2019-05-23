#include "extensions/common/wasm/null/null.h"
#include "stackdriver.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
namespace Stackdriver {
NullVmPluginRootRegistry* context_registry_{};
}  // namespace Stackdriver

/**
 * Config registration for a Wasm filter plugin. @see
 * NamedHttpFilterConfigFactory.
 */
class StackdriverPluginFactory : public NullVmPluginFactory {
 public:
  StackdriverPluginFactory() {}

  const std::string name() const override { return "stackdriver_plugin"; }
  std::unique_ptr<NullVmPlugin> create() const override {
    return std::make_unique<NullVmPlugin>(
        Envoy::Extensions::Common::Wasm::Null::Plugin::Stackdriver::
            context_registry_);
  }
};

/**
 * Static registration for the null Wasm filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<StackdriverPluginFactory, NullVmPluginFactory>
    register_;

}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
