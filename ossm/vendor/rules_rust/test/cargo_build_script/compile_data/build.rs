use std::path::PathBuf;

fn main() {
    let path = "data.txt";
    if !PathBuf::from(path).exists() {
        panic!("File does not exist in path.");
    }
    println!("cargo:rustc-env=DATA={}", path);
}
