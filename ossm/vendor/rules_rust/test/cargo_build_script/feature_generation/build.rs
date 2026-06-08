//! A build.rs script which produces a feature for the consuming crate.

fn main() {
    println!("cargo:rustc-cfg=build_rs_generated_feature");
}
