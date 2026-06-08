extern "C" {
    pub static defined_by_linkstamp: std::os::raw::c_int;
}
#[proc_macro]
pub fn num_from_linkstamp(_: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let tt: proc_macro::TokenTree =
        proc_macro::Literal::u32_suffixed(unsafe { defined_by_linkstamp } as u32).into();
    tt.into()
}
