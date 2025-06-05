#include <datadog/cerr_logger.h>
#include <datadog/error.h>

#include <ios>
#include <iostream>
#include <ostream>
#include <sstream>

#include "test.h"

using namespace datadog::tracing;

namespace {

// Replace the `streambuf` associated with a specified `std::ios` for the
// lifetime of this object.  Restore the previous `streambuf` afterward.
class StreambufGuard {
  std::ios *stream_;
  std::streambuf *buffer_;

 public:
  StreambufGuard(std::ios &stream, std::streambuf *buffer)
      : stream_(&stream), buffer_(stream.rdbuf()) {
    stream.rdbuf(buffer);
  }

  ~StreambufGuard() { stream_->rdbuf(buffer_); }
};

}  // namespace

// `CerrLogger` is the default logger.
// These test exist just to cover all of its methods.
TEST_CASE("CerrLogger") {
  std::ostringstream stream;
  const StreambufGuard guard{std::cerr, stream.rdbuf()};
  CerrLogger logger;

  SECTION("log_error func") {
    logger.log_error([](std::ostream &stream) { stream << "hello!"; });
    // Note the appended newline.
    REQUIRE(stream.str() == "hello!\n");
  }

  SECTION("log_startup func") {
    logger.log_startup([](std::ostream &stream) { stream << "hello!"; });
    REQUIRE(stream.str() == "hello!\n");
  }

  SECTION("log_error Error") {
    logger.log_error(Error{Error::OTHER, "hello!"});
    REQUIRE(stream.str() == "[dd-trace-cpp error code 1] hello!\n");
  }

  SECTION("log_error string_view") {
    logger.log_error("hello!");
    REQUIRE(stream.str() == "hello!\n");
  }
}
