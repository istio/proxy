use core::intrinsics;
use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    intrinsics::abort()
}

#[alloc_error_handler]
#[allow(clippy::panic)]
fn default_handler(layout: core::alloc::Layout) -> ! {
    panic!("memory allocation of {} bytes failed", layout.size())
}

#[lang = "eh_personality"]
extern "C" fn eh_personality() {}
