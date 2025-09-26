//! A tool for installing bootstrapped Rust binaries into the requested paths.

use std::{
    env,
    fs::{copy, create_dir_all},
    path::PathBuf,
};

fn install() -> std::io::Result<u64> {
    let binary = PathBuf::from(env!("RULES_RUST_CARGO_BOOTSTRAP_BINARY"));

    // Consume only the first argument as the destination
    let dest = PathBuf::from(
        env::args()
            .nth(1)
            .expect("No destination argument provided"),
    );

    // Create the parent directory structure if it doesn't exist
    if let Some(parent) = dest.parent() {
        if !parent.exists() {
            create_dir_all(parent)?;
        }
    }

    // Copy the file to the requested destination
    copy(binary, dest)
}

fn main() {
    if let Err(err) = install() {
        eprintln!("{err:?}");
        std::process::exit(1);
    };
}
