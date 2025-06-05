#pragma once

// This component provides an interface, `Logger`, that allows for the
// customization of how the library logs diagnostic and startup messages.
//
// Errors, when they occur, are typically returned as `Error` values (often as
// part of an `Expected` value). However, in "async" contexts where there is
// nowhere to return a value, the logger is used instead.
//
// `Logger`'s pure virtual member functions accept a callback function that is
// either invoked immediately or not invoked at all, depending on the
// implementation. Use of a callback is a compromise between always paying the
// overhead of forming log messages, and using the preprocessor to obscure a
// branch.
//
// The callback function accepts a single `std::ostream&` argument. Text
// inserted into the stream will appear in the resulting log message (assuming
// that an implementation invokes the callback). It is not necessary to append
// a newline character to the stream; an implementation will do that.
//
//     if (const int rcode = errno) {
//       logger.log_error([rcode](std::ostream& log) {
//         log << "Doodad frobnication failed: " << std::strerror(rcode);
//       });
//     }
//
// Non-pure virtual overloads of `log_error` are provided for convenience. One
// overload accepts an `Error`:
//
//     if (Error *error = expected_cake.if_error()) {
//       logger.log_error(*error);
//       return;
//     }
//
// The other overload accepts a `StringView`:
//
//     if (!success) {
//       logger.log_error("Something went wrong with the frobnication.");
//       return;
//     }

#include <functional>
#include <ostream>

#include "string_view.h"

namespace datadog {
namespace tracing {

struct Error;

class Logger {
 public:
  using LogFunc = std::function<void(std::ostream&)>;

  virtual ~Logger() {}

  virtual void log_error(const LogFunc&) = 0;
  virtual void log_startup(const LogFunc&) = 0;

  virtual void log_error(const Error&);
  virtual void log_error(StringView);
};

}  // namespace tracing
}  // namespace datadog
