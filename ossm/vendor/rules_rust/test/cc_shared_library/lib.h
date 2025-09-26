#include <stdint.h>

#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif

DllExport int32_t foo();
