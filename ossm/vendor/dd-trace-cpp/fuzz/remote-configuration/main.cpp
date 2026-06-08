#include <datadog/logger.h>
#include <datadog/remote_config/remote_config.h>
#include <datadog/string_view.h>

#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>

namespace dd = datadog;

class NoopLogger : public dd::tracing::Logger {
  std::mutex mutex_;
  std::ostringstream stream_;

 public:
  void log_error(const LogFunc&) override{};
  void log_startup(const LogFunc&) override{};
  using Logger::log_error;  // expose the non-virtual overloads

 private:
  void log(const LogFunc&){};
};

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, size_t size) {
  dd::tracing::TracerSignature signature(dd::tracing::RuntimeID::generate(),
                                         "foosvc", "fooenv");
  std::vector<std::shared_ptr<dd::remote_config::Listener>> listener;
  auto logger = std::make_shared<NoopLogger>();
  datadog::remote_config::Manager manager(signature, listener, logger);
  std::string in((char*)data, size);
  auto j = nlohmann::json::parse(in, nullptr, false);
  if (j.is_discarded()) return 0;

  manager.process_response(j);
  return 0;
}
