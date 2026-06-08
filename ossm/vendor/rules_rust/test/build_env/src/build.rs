fn main() {
    println!(
        "cargo:rustc-env=CARGO_PKG_NAME_FROM_BUILD_SCRIPT={}",
        env!("CARGO_PKG_NAME")
    );
    println!(
        "cargo:rustc-env=CARGO_CRATE_NAME_FROM_BUILD_SCRIPT={}",
        env!("CARGO_CRATE_NAME")
    );
    println!("cargo:rustc-env=HAS_TRAILING_SLASH=foo\\");
}
