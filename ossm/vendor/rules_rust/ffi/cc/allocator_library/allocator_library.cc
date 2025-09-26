#include <stdint.h>

// This file has some exciting magic to get Rust code linking in a cc_binary.
// The Rust compiler generates some similar symbol aliases when it links, so we
// have to do it manually. We mark all our symbols as weak so that linking this
// via Rust tooling to produce a binary with a Rust main works.
//
// It is intended to be used in rust_toolchain.allocator_library.
//
// https://github.com/rust-lang/rust/blob/master/library/alloc/src/alloc.rs
// and https://github.com/rust-lang/rust/blob/master/library/std/src/alloc.rs
// are the best source of docs I've found on these functions and variables.
// https://doc.rust-lang.org/std/alloc/index.html talks about how this is
// intended to be used.
//
// Also note
// https://rust-lang.github.io/unsafe-code-guidelines/layout/scalars.html for
// the sizes of the various integer types.
//
// This file strongly assumes that the default allocator is used. It will
// not work with any other allocated switched in via `#[global_allocator]`.

// New feature as of https://github.com/rust-lang/rust/pull/88098.
__attribute__((weak)) uint8_t __rust_alloc_error_handler_should_panic = 0;

extern "C" uint8_t *__rdl_alloc(uintptr_t size, uintptr_t align);
extern "C" __attribute__((weak)) uint8_t *__rust_alloc(uintptr_t size,
                                                       uintptr_t align) {
    return __rdl_alloc(size, align);
}
extern "C" void __rdl_dealloc(uint8_t *ptr, uintptr_t size, uintptr_t align);
extern "C" __attribute__((weak)) void __rust_dealloc(uint8_t *ptr,
                                                     uintptr_t size,
                                                     uintptr_t align) {
    __rdl_dealloc(ptr, size, align);
}
extern "C" uint8_t *__rdl_realloc(uint8_t *ptr, uintptr_t old_size,
                                  uintptr_t align, uintptr_t new_size);
extern "C" __attribute__((weak)) uint8_t *__rust_realloc(uint8_t *ptr,
                                                         uintptr_t old_size,
                                                         uintptr_t align,
                                                         uintptr_t new_size) {
    return __rdl_realloc(ptr, old_size, align, new_size);
}
extern "C" uint8_t *__rdl_alloc_zeroed(uintptr_t size, uintptr_t align);
extern "C" __attribute__((weak)) uint8_t *__rust_alloc_zeroed(uintptr_t size,
                                                              uintptr_t align) {
    return __rdl_alloc_zeroed(size, align);
}
extern "C" void __rdl_oom(uintptr_t size, uintptr_t align);
extern "C" __attribute__((weak)) void __rust_alloc_error_handler(
    uintptr_t size, uintptr_t align) {
    __rdl_oom(size, align);
}

// New requirement as of Rust 1.71.0. For more details see
// https://github.com/rust-lang/rust/issues/73632.
__attribute__((weak)) uint8_t __rust_no_alloc_shim_is_unstable = 0;
