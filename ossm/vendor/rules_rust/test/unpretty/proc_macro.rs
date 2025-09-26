// This differs from the edition 2015 version because it does not have an `extern proc_macro`
// statement, which became optional in edition 2018.

use proc_macro::TokenStream;

#[proc_macro]
pub fn make_answer(_item: TokenStream) -> TokenStream {
    "fn answer() -> u32 { 42 }".parse().unwrap()
}
