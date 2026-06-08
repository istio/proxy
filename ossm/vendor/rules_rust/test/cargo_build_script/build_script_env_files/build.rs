fn main() {
    println!(
        "cargo:rustc-env=FROM_BUILD_SCRIPT={}",
        std::env::var("GREETING").unwrap()
    );
}
