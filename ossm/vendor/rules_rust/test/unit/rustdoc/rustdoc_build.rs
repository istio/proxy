fn main() {
    println!("cargo:rustc-env=CONST=xyz");
    if cfg!(target_os = "macos") {
        println!("cargo:rustc-link-lib=dylib=c++")
    } else if cfg!(target_os = "linux") {
        println!("cargo:rustc-link-lib=dylib=stdc++")
    }
}
