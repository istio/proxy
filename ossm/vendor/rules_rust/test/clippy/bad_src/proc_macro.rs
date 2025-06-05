extern crate proc_macro;
use proc_macro::TokenStream;

#[proc_macro]
pub fn make_answer(_item: TokenStream) -> TokenStream {
    assert!(true);
    loop {
        println!("{}", "Hello World");
        break;
    }
    "fn answer() -> u32 { 42 }".parse().unwrap()
}
