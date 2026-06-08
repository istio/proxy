fn main() {
    println!("CARGO_PKG_VERSION={}", env!("CARGO_PKG_VERSION"));
    println!(
        "CARGO_PKG_VERSION_MAJOR={}",
        env!("CARGO_PKG_VERSION_MAJOR")
    );
    println!(
        "CARGO_PKG_VERSION_MINOR={}",
        env!("CARGO_PKG_VERSION_MINOR")
    );
    println!(
        "CARGO_PKG_VERSION_PATCH={}",
        env!("CARGO_PKG_VERSION_PATCH")
    );
    println!("CARGO_PKG_VERSION_PRE={}", env!("CARGO_PKG_VERSION_PRE"));
    println!("CARGO_PKG_NAME={}", env!("CARGO_PKG_NAME"));
    println!("CARGO_PKG_AUTHORS={}", env!("CARGO_PKG_AUTHORS"));
    println!("CARGO_PKG_DESCRIPTION={}", env!("CARGO_PKG_DESCRIPTION"));
    println!("CARGO_PKG_HOMEPAGE={}", env!("CARGO_PKG_HOMEPAGE"));
    println!("CARGO_PKG_REPOSITORY={}", env!("CARGO_PKG_REPOSITORY"));
    println!("CARGO_PKG_LICENSE={}", env!("CARGO_PKG_LICENSE"));
    println!("CARGO_PKG_LICENSE_FILE={}", env!("CARGO_PKG_LICENSE_FILE"));
    println!("CARGO_PKG_RUST_VERSION={}", env!("CARGO_PKG_RUST_VERSION"));
    println!("CARGO_PKG_README={}", env!("CARGO_PKG_README"));
}
