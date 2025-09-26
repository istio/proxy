use std::env::var;
use std::fs;
use std::io::Result;
use std::path::PathBuf;

fn main() {
    let dep_dir = PathBuf::from(var("DEP_DIR").expect("DEP_DIR should be set").trim());
    let entries = fs::read_dir(&dep_dir)
        .unwrap_or_else(|e| {
            panic!(
                "Failed to open DEP_DIR directory: {}\n{:?}",
                dep_dir.display(),
                e
            )
        })
        .collect::<Result<Vec<_>>>()
        .expect("Failed to read DEP_DIR directory entries");
    let entries = entries
        .iter()
        .map(|entry| {
            entry
                .path()
                .file_name()
                .unwrap()
                .to_string_lossy()
                .to_string()
        })
        .collect::<Vec<_>>();
    assert_eq!(entries, vec!["a_file".to_string()]);
}
