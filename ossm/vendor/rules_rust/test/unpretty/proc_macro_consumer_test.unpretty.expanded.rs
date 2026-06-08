#![feature(prelude_import)]
extern crate std;
#[prelude_import]
use std::prelude::rust_2021::*;
use proc_macro::make_answer;

fn answer() -> u32 { 42 }

extern crate test;
#[rustc_test_marker = "test_answer_macro"]
#[doc(hidden)]
pub const test_answer_macro: test::TestDescAndFn =
    test::TestDescAndFn {
        desc: test::TestDesc {
            name: test::StaticTestName("test_answer_macro"),
            ignore: false,
            ignore_message: ::core::option::Option::None,
            source_file: "test/unpretty/proc_macro_consumer.rs",
            start_line: 6usize,
            start_col: 4usize,
            end_line: 6usize,
            end_col: 21usize,
            compile_fail: false,
            no_run: false,
            should_panic: test::ShouldPanic::No,
            test_type: test::TestType::Unknown,
        },
        testfn: test::StaticTestFn(#[coverage(off)] ||
                test::assert_test_result(test_answer_macro())),
    };
fn test_answer_macro() {
    { ::std::io::_print(format_args!("{0}\n", answer())); };
}
#[rustc_main]
#[coverage(off)]
#[doc(hidden)]
pub fn main() -> () {
    extern crate test;
    test::test_main_static(&[&test_answer_macro])
}
