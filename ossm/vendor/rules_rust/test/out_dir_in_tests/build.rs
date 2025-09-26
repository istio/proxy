use std::env;
use std::fs::File;
use std::io::prelude::*;
use std::path::PathBuf;

fn main() -> std::io::Result<()> {
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    let mut file = File::create(out_path.join("test_content.txt"))?;
    file.write_all(b"Test content")?;
    Ok(())
}
