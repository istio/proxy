#pragma once

#include <string>

namespace Envoy {
namespace Utils {

// Zipkin B3 headers
const std::string kTraceID = "x-b3-traceid";
const std::string kSpanID = "x-b3-spanid";
const std::string kParentSpanID = "x-b3-parentspanid";
const std::string kSampled = "x-b3-sampled";

const std::set<std::string> TracingHeaderSet = {
    kTraceID, kSpanID, kParentSpanID, kSampled,
};

}  // namespace Utils
}  // namespace Envoy
