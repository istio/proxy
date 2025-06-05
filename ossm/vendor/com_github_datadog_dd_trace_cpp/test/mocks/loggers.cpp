#include "loggers.h"

#include <cstddef>
#include <ostream>

std::ostream& operator<<(std::ostream& stream,
                         const std::vector<MockLogger::Entry>& entries) {
  stream << "<BEGIN " << entries.size() << " LOG ENTRIES>";
  std::size_t i = 0;
  for (; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    const auto kind_name =
        entry.kind == MockLogger::Entry::DD_ERROR ? "ERROR" : "STARTUP";
    stream << '\n' << (i + 1) << ". " << kind_name << ": ";
    std::visit([&](const auto& value) { stream << value; }, entry.payload);
  }
  if (i) {
    stream << '\n';
  }
  return stream << "</END " << entries.size() << " LOG ENTRIES>";
}
