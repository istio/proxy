extern crate proc_macro;

use proc_macro::TokenStream;

#[proc_macro]
pub fn make_answer(_item: TokenStream) -> TokenStream {
    let answer = proc_macro_dep::proc_macro_dep();
    format!("fn answer() -> u32 {{ {answer} }}")
        .parse()
        .unwrap()
}
