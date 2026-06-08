__attribute__((export_name("strlen")))
unsigned long wasm_strlen(char *s) {
	unsigned long len = 0;
	for (; *s; len++) {}
	return len;
}
