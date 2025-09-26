#pragma once

#include "fmt/format.h"
#include <iostream>
namespace Hessian2 {

#ifdef HESSIAN_LOG_OUTPUT
#define HESSIAN_LOG(level, args...)                                       \
  do {                                                                    \
    std::cerr << "" << #level << ":\t" << fmt::format(args) << std::endl; \
  } while (0)
#else
#define HESSIAN_LOG(level, args...)
#endif

}  // namespace Hessian2
