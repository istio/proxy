#include <stdint.h>
#include <string.h>

__attribute__((export_name("strlen")))
uint32_t wasm_strlen(char *s) {
	return strlen(s);
}
