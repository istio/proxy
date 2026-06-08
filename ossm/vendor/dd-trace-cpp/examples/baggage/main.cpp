#include <datadog/dict_reader.h>
#include <datadog/tracer.h>

#include <iostream>

namespace dd = datadog::tracing;

struct CinReader : public dd::DictReader {
  std::string input;

  dd::Optional<dd::StringView> lookup(dd::StringView key) const override {
    return input;
  }

  void visit(
      const std::function<void(dd::StringView key, dd::StringView value)>&
          visitor) const override{};
};

std::istream& operator>>(std::istream& is, CinReader& reader) {
  is >> reader.input;
  return is;
}

std::ostream& operator<<(std::ostream& os, dd::Baggage::Error error) {
  using dd::Baggage;
  switch (error.code) {
    case Baggage::Error::MISSING_HEADER:
      os << "missing `baggage` header";
      break;
    case Baggage::Error::MALFORMED_BAGGAGE_HEADER: {
      os << "malformed `baggage` header";
      if (error.pos) {
        os << " at position " << *error.pos;
      }
    } break;
    case Baggage::Error::MAXIMUM_CAPACITY_REACHED:
      os << "maximum number of bagge items reached";
      break;
    case Baggage::Error::MAXIMUM_BYTES_REACHED:
      os << "maximum amount of bytes written";
      break;
    default:
      os << "unknown error code";
      break;
  }
  return os;
}

int main() {
  dd::TracerConfig cfg;
  cfg.log_on_startup = false;
  cfg.telemetry.enabled = false;
  cfg.agent.remote_configuration_enabled = false;
  const auto finalized_cfg = datadog::tracing::finalize_config(cfg);
  if (auto error = finalized_cfg.if_error()) {
    std::cerr << "Failed to initialize the tracer: " << error->message
              << std::endl;
    return error->code;
  }

  dd::Tracer tracer(*finalized_cfg);

  std::cout
      << "This program demonstrates how to use baggage, a feature that allows "
         "metadata (key-value pairs) to be attached to a request and "
         "propagated across services.\n"
         "Baggage can be useful for passing contextual information, such as "
         "user IDs, session tokens, or request attributes, between different "
         "components of a distributed system.\n\n"
         "This example lets you input baggage values, validate them and "
         "displays the baggage content parsed.\n"
         "You can enter baggage manually or provide it through a file, try:\n"
         "- k1=v1,k2=v2\n"
         "- ,invalid=input\n"
         "or ./baggage-example < list-of-baggages.txt\n\n";

  CinReader reader;
  std::cout << "Enter baggage (or 'CTRL+C' to quit): ";
  while (std::getline(std::cin, reader.input)) {
    auto baggage = tracer.extract_baggage(reader);
    if (!baggage) {
      std::cout << "Error parsing \"" << reader.input
                << "\": " << baggage.error() << ".\n";
    } else {
      std::cout << "Baggage key-value parsed: \n";
      baggage->visit([](dd::StringView key, dd::StringView value) {
        std::cout << key << ": " << value << std::endl;
      });
    }

    std::cout << "\nEnter baggage (or 'CTRL+C' to quit): ";
  }
  return 0;
}
