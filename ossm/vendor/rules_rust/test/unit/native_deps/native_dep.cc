int native_global = 42;

extern "C" int native_dep() { return native_global; }