#![no_std]
#![no_main]
#![feature(lang_items)]

use core::arch::asm;
use core::arch::global_asm;
use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    exit(1);
}

global_asm! {
    ".global _start",
    "_start:",
    "mov rdi, rsp",
    "call start_main"
}

fn exit(status: i32) -> ! {
    unsafe {
        asm!(
            "syscall",
            in("rax") 60,
            in("rdi") status,
            options(noreturn)
        );
    }
}

#[no_mangle]
unsafe fn start_main(_stack_top: *const u8) -> ! {
    exit(0);
}

#[lang = "eh_personality"]
extern "C" fn eh_personality() {}
