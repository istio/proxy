use proc_macro::TokenStream;

#[proc_macro]
pub fn id(input: TokenStream) -> TokenStream {
    input
}
