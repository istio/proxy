use proc_macro::TokenStream;

extern "C" {
    fn native_dep() -> isize;
}
#[proc_macro_derive(UsingNativeDep)]
pub fn use_native_dep(_input: TokenStream) -> TokenStream {
    println!("{}", unsafe { native_dep() });
    panic!("done")
}
