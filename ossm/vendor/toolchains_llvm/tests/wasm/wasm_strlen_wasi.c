#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint32_t wasm_strlen(char *s) {
	return strlen(s);
}

int main() {
	char *s = "Hello world!";
	printf("strlen(\"%s\") = %d\n", s, wasm_strlen(s));
	return 0;
}
