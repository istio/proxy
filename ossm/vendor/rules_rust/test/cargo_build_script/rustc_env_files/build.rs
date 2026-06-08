fn main() {
    println!("cargo:rustc-env=FROM_BUILD_SCRIPT={}", env!("GREETING"));
}
