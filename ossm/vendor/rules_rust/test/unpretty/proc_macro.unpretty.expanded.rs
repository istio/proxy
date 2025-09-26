#![feature(prelude_import)]
#[prelude_import]
use std::prelude::rust_2021::*;
#[macro_use]
extern crate std;
// This differs from the edition 2015 version because it does not have an `extern proc_macro`
// statement, which became optional in edition 2018.

use proc_macro::TokenStream;

#[proc_macro]
pub fn make_answer(_item: TokenStream) -> TokenStream {
    "fn answer() -> u32 { 42 }".parse().unwrap()
}
const _: () =
    {
        extern crate proc_macro;
        #[rustc_proc_macro_decls]
        #[used]
        #[allow(deprecated)]
        static _DECLS: &[proc_macro::bridge::client::ProcMacro] =
            &[proc_macro::bridge::client::ProcMacro::bang("make_answer",
                            make_answer)];
    };
