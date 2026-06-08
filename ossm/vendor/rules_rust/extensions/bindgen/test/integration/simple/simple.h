#ifndef __SIMPLE_H_INCLUDE__
#define __SIMPLE_H_INCLUDE__

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

#include <stdint.h>

#ifdef SIMPLE_DEFINE

static const int64_t SIMPLE_VALUE = 42;

#endif

EXTERN_C const int64_t simple_function();

static inline int64_t simple_static_function() { return 84; }

#endif
