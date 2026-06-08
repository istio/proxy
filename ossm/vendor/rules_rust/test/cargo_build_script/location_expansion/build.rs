fn main() {
    println!(
        "cargo:rustc-env=DATA_ROOTPATH={}",
        std::env::var("DATA_ROOTPATH").expect("Environment variable not set")
    );
    println!(
        "cargo:rustc-env=DATA_EXECPATH={}",
        std::env::var("DATA_EXECPATH").expect("Environment variable not set")
    );
    println!(
        "cargo:rustc-env=DATA_RLOCATIONPATH={}",
        std::env::var("DATA_RLOCATIONPATH").expect("Environment variable not set")
    );
    println!(
        "cargo:rustc-env=TOOL_ROOTPATH={}",
        std::env::var("TOOL_ROOTPATH").expect("Environment variable not set")
    );
    println!(
        "cargo:rustc-env=TOOL_EXECPATH={}",
        std::env::var("TOOL_EXECPATH").expect("Environment variable not set")
    );
    println!(
        "cargo:rustc-env=TOOL_RLOCATIONPATH={}",
        std::env::var("TOOL_RLOCATIONPATH").expect("Environment variable not set")
    );
}
