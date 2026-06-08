#include <datadog/remote_config/product.h>

#include "string_util.h"

#define DD_QUOTED_IMPL(ARG) #ARG
#define DD_QUOTED(ARG) DD_QUOTED_IMPL(ARG)

namespace datadog {
namespace remote_config {

tracing::StringView to_string_view(product::Flag product) {
#define X(NAME, ID)           \
  case product::Flag::NAME: { \
    return DD_QUOTED(NAME);   \
  }
  switch (product) { DD_LIST_REMOTE_CONFIG_PRODUCTS }
#undef X

  assert(true);
  return "UNKNOWN";
}

product::Flag parse_product(tracing::StringView sv) {
  const auto upcase_product = tracing::to_upper(sv);

#define X(NAME, ID)                        \
  if (upcase_product == DD_QUOTED(NAME)) { \
    return product::Flag::NAME;            \
  }
  DD_LIST_REMOTE_CONFIG_PRODUCTS
#undef X

  assert(true);
  return product::Flag::UNKNOWN;
}

void visit_products(Products products,
                    std::function<void(product::Flag)> on_product) {
#define X(NAME, ID)                     \
  if (products & product::Flag::NAME) { \
    on_product(product::Flag::NAME);    \
  }

  DD_LIST_REMOTE_CONFIG_PRODUCTS
#undef X
}

}  // namespace remote_config
}  // namespace datadog

#undef DD_QUOTED
#undef DD_QUOTED_IMPL
