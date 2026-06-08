#pragma once

#include <string>

#include "capability.h"
#include "datadog/optional.h"
#include "product.h"

namespace datadog {
namespace remote_config {

// Interface for handling remote configuration notifications
//
// The `Listener` class provides an interface for handling configuration
// updates and reverts for the set of products and capabilities it subscribes
// to.
class Listener {
 public:
  struct Configuration {
    std::string id;
    std::string path;
    std::string content;
    std::size_t version;
    product::Flag product;
  };

  virtual ~Listener() = default;

  // Retrieves the list of products the listener wants to subscribe.
  virtual Products get_products() = 0;

  // Retrieve the list of capabilities the listener wants to subscribe.
  virtual Capabilities get_capabilities() = 0;

  // Pure virtual function called when a configuration needs to be reverted.
  virtual void on_revert(const Configuration&) = 0;

  // Pure virtual function called when a configuration is updated.
  // Returns an error message if the configuration could not be applied, or
  // nothing.
  virtual tracing::Optional<std::string> on_update(const Configuration&) = 0;

  // TODO: Find a better name and a better solution! Mainly here for ASM
  // Called once the last remote configuration response is completed.
  virtual void on_post_process() = 0;
};

}  // namespace remote_config
}  // namespace datadog
