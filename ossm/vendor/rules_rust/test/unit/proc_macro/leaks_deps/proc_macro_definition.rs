use proc_macro::TokenStream;
use proc_macro_dep::forty_two;

#[proc_macro]
pub fn make_forty_two(_item: TokenStream) -> TokenStream {
    forty_two().to_string().parse().unwrap()
}
