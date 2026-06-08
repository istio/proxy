fn main() {
    println!(
        "cargo:rustc-env=BUILD_SCRIPT_OPT_LEVEL={}",
        std::env::var("OPT_LEVEL").unwrap()
    );
    println!(
        "cargo:rustc-env=BUILD_SCRIPT_OUT_DIR={}",
        std::env::var("OUT_DIR").unwrap()
    );
}
