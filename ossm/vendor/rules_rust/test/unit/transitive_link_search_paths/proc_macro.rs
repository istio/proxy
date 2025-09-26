extern crate proc_macro;

use proc_macro::TokenStream;

#[proc_macro_attribute]
pub fn yolo(args: TokenStream, _input: TokenStream) -> TokenStream {
    args
}
