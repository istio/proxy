#include <datadog/baggage.h>

namespace datadog {
namespace tracing {

namespace {

/// Whitespace in RFC 7230 section 3.2.3 definition
/// BDNF:
///  - OWS  = *(SP / HTAB)
///  - SP   = SPACE (0x20)
///  - HTAB = Horizontal tab (0x09)
constexpr bool is_whitespace(char c) { return c == 0x20 || c == 0x09; }

constexpr bool is_allowed_key_char(char c) {
  // clang-format off
  return (c >= 0x30 && c <= 0x39)   ///< [0-9]
      || (c >= 0x41 && c <= 0x5A)   ///< [a-z]
      || (c >= 0x61 && c <= 0x7A)   ///< [A-Z]
      || (c == 0x21)                ///< "!"
      || (c >= 0x23 && c <= 0x27)   ///< "#" / "$" / "%" / "&" / "'"
      || (c == 0x2A)                ///< "*"
      || (c == 0x2B)                ///< "+"
      || (c == 0x2D)                ///< "-"
      || (c == 0x2E)                ///< "."
      || (c == 0x5E)                ///< "^"
      || (c == 0x5F)                ///< "_"
      || (c == 0x60)                ///< "`"
      || (c == 0x7C)                ///< "|"
      || (c == 0x7E);               ///< "~"
  // clang-format on
}

constexpr bool is_allowed_value_char(char c) {
  // clang-format off
  return (c == 0x21)                ///< "!"
      || (c >= 0x23 && c <= 0x2B)   ///< "#" / "$" / "%" / "&" / "'" / "(" / /< ")" / "*" / "+" / "," / "-"
      || (c >= 0x2D && c <= 0x5B)   ///< "-" / "." / "/" / [0-9] / ";' / "<" / "=" / ">" / "?" / "@" / [A-Z]
      || (c >= 0x5D && c <= 0x7E);  ///< "]" / "^" / "_" / "`" / [a-z]
  // clang-format on
}

Expected<std::unordered_map<std::string, std::string>, Baggage::Error>
parse_baggage(StringView input) {
  std::unordered_map<std::string, std::string> result;
  if (input.empty()) return result;

  enum class state : char {
    leading_spaces_key,
    key,
    trailing_spaces_key,
    leading_spaces_value,
    value,
    trailing_spaces_value,
    properties,
  } internal_state = state::leading_spaces_key;

  size_t beg = 0;
  size_t tmp_end = 0;

  StringView key;
  StringView value;

  const size_t end = input.size();

  for (size_t i = 0; i < end; ++i) {
    auto c = input[i];

    switch (internal_state) {
      case state::leading_spaces_key: {
        if (!is_whitespace(c)) {
          beg = i;
          tmp_end = i;
          internal_state = state::key;
          goto key;
        }
      } break;

      case state::key: {
      key:
        if (c == '=') {
          tmp_end = i;
          goto consume_key;
        } else if (is_whitespace(c)) {
          tmp_end = i;
          internal_state = state::trailing_spaces_key;
        } else if (!is_allowed_key_char(c)) {
          return Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER, i};
        }
      } break;

      case state::trailing_spaces_key: {
        if (c == '=') {
        consume_key:
          size_t count = tmp_end - beg;
          if (count < 1)
            return Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER, i};

          key = StringView{input.data() + beg, count};
          internal_state = state::leading_spaces_value;
        } else if (!is_whitespace(c)) {
          return Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER, i};
        }
      } break;

      case state::leading_spaces_value: {
        if (!is_whitespace(c)) {
          beg = i;
          tmp_end = i;
          internal_state = state::value;
          goto value;
        }
      } break;

      case state::value: {
      value:
        if (c == ',') {
          tmp_end = i;
          goto consume_value;
        } else if (c == ';') {
          tmp_end = i;
          internal_state = state::properties;
        } else if (is_whitespace(c)) {
          tmp_end = i;
          internal_state = state::trailing_spaces_value;
        } else if (!is_allowed_value_char(c)) {
          return Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER, i};
        }
      } break;

      case state::properties: {
        if (c == ',') {
          goto consume_value;
        }
      } break;

      case state::trailing_spaces_value: {
        if (c == ',') {
        consume_value:
          size_t count = tmp_end - beg;
          if (count < 1)
            return Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER,
                                  tmp_end};

          value = StringView{input.data() + beg, count};
          result.emplace(std::string(key), std::string(value));
          beg = i;
          tmp_end = i;
          internal_state = state::leading_spaces_key;
        } else if (c == ';') {
          internal_state = state::properties;
        } else if (!is_whitespace(c)) {
          return Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER, i};
        }
      } break;
    }
  }

  if (internal_state == state::value) {
    value = StringView{input.data() + beg, end - beg};
    result.emplace(std::string(key), std::string(value));
  } else if (internal_state == state::trailing_spaces_value ||
             internal_state == state::properties) {
    value = StringView{input.data() + beg, tmp_end - beg};
    result.emplace(std::string(key), std::string(value));
  } else {
    return Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER, end};
  }

  return result;
}

}  // namespace

Baggage::Baggage(size_t max_capacity) : max_capacity_(max_capacity) {
  (void)max_capacity_;
}

Baggage::Baggage(std::unordered_map<std::string, std::string> baggage,
                 size_t max_capacity)
    : max_capacity_(max_capacity), baggage_(std::move(baggage)) {}

Optional<StringView> Baggage::get(StringView key) const {
  auto it = baggage_.find(std::string(key));
  if (it == baggage_.cend()) return nullopt;

  return it->second;
}

bool Baggage::set(std::string key, std::string value) {
  baggage_[key] = value;
  return true;
}

void Baggage::remove(StringView key) { baggage_.erase(std::string(key)); }

void Baggage::clear() { baggage_.clear(); }

size_t Baggage::size() const { return baggage_.size(); }

bool Baggage::empty() const { return baggage_.empty(); }

bool Baggage::contains(StringView key) const {
  auto found = baggage_.find(std::string(key));
  return found != baggage_.cend();
}

void Baggage::visit(std::function<void(StringView, StringView)>&& visitor) {
  for (const auto& [key, value] : baggage_) {
    visitor(key, value);
  }
}

Expected<void> Baggage::inject(DictWriter& writer, const Options& opts) const {
  auto n = baggage_.size();
  if (n == 0) return {};

  Expected<void> res;
  if (n > opts.max_items) {
    std::string err_msg = "injected ";
    err_msg += std::to_string(opts.max_items);
    err_msg += " out of ";
    err_msg += std::to_string(baggage_.size());
    err_msg += " baggage items";
    res = datadog::tracing::Error{
        datadog::tracing::Error::Code::BAGGAGE_MAXIMUM_ITEMS_REACHED, err_msg};
  }

  // TODO(@dmehala): Memory alloc optimization, (re)use fixed size buffer.
  std::string seralized_baggage;
  seralized_baggage.reserve(opts.max_bytes);

  auto it = baggage_.cbegin();
  seralized_baggage += it->first;
  seralized_baggage += "=";
  seralized_baggage += it->second;
  if (seralized_baggage.size() > opts.max_bytes) {
    return datadog::tracing::Error{
        datadog::tracing::Error::Code::BAGGAGE_MAXIMUM_BYTES_REACHED,
        "reached maximum bytes size limit"};
  }

  size_t items = 1;
  for (it++; it != baggage_.cend() && ++items < opts.max_items; ++it) {
    std::string buffer;
    buffer += ",";
    buffer += it->first;
    buffer += "=";
    buffer += it->second;

    if (buffer.size() + seralized_baggage.size() > opts.max_bytes) {
      res = datadog::tracing::Error{
          datadog::tracing::Error::Code::BAGGAGE_MAXIMUM_BYTES_REACHED,
          "reached maximum bytes size limit"};
      break;
    }

    seralized_baggage += buffer;
  }

  /// NOTE(@dmehala): It is the writer's responsibility to write the header,
  /// including percent-encoding.
  writer.set("baggage", seralized_baggage);
  return res;
}

Expected<Baggage, Baggage::Error> Baggage::extract(const DictReader& headers) {
  auto found = headers.lookup("baggage");
  if (!found) {
    return Baggage::Error{Error::MISSING_HEADER};
  }

  // TODO(@dmehala): Avoid allocation
  auto bv = parse_baggage(*found);
  if (auto error = bv.if_error()) {
    return *error;
  }

  Baggage result(*bv);
  return result;
}

}  // namespace tracing
}  // namespace datadog
