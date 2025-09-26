fn main() {
    let contents = std::fs::read_to_string("test/cargo_build_script/run_from_exec_root/data.txt")
        .expect("Failed to read data file");
    println!("cargo:rustc-env=DATA={}", contents);
}
