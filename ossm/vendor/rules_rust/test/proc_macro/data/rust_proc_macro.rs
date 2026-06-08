use proc_macro::TokenStream;
use std::path::Path;

#[proc_macro]
pub fn ensure_proc_macro_data_exists(_input: TokenStream) -> TokenStream {
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("proc_macro_data.txt");
    assert!(path.exists(), "not found: {}", path.display());

    proc_macro_helper::ensure_helper_data_exists();

    TokenStream::new()
}
