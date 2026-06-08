//! A tool for copying files and avoiding
//! https://github.com/bazelbuild/bazel/issues/21747

use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let src = PathBuf::from(std::env::args().nth(1).expect("No source file provided"));
    let dest = PathBuf::from(env::args().nth(2).expect("No destination provided"));

    fs::copy(&src, &dest).unwrap_or_else(|e| {
        panic!(
            "Failed to copy file `{} -> {}`\n{:?}",
            src.display(),
            dest.display(),
            e
        )
    });
}
