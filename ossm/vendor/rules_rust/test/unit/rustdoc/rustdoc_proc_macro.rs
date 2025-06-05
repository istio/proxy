use proc_macro::TokenStream;

#[proc_macro]
pub fn make_answer(_item: TokenStream) -> TokenStream {
    "\
/// A procedural macro example:
/// ```
/// fn answer() -> u32 { 42 }
/// assert_eq!(answer(), 42);
/// ```
pub fn answer() -> u32 { 42 }"
        .parse()
        .unwrap()
}
