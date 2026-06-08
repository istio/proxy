#include <datadog/base64.h>
#include <datadog/string_view.h>

#include <cstdint>

namespace dd = datadog::tracing;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, size_t size) {
  dd::base64_decode(dd::StringView{(const char*)data, size});
  return 0;
}
