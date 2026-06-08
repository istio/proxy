use b::hello;
use proc_macro::{Literal, TokenStream, TokenTree};

#[proc_macro]
pub fn greet(_item: TokenStream) -> TokenStream {
    TokenTree::Literal(Literal::string(hello())).into()
}
