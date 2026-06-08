use std::path::{Path, PathBuf};

#[cfg(target_family = "unix")]
fn symlink(original: impl AsRef<Path>, link: impl AsRef<Path>) {
    std::os::unix::fs::symlink(original, link).unwrap();
}

#[cfg(target_family = "windows")]
fn symlink(original: impl AsRef<Path>, link: impl AsRef<Path>) {
    std::os::windows::fs::symlink_file(original, link).unwrap();
}

fn main() {
    let path = "data.txt";
    if !PathBuf::from(path).exists() {
        panic!("File does not exist in path.");
    }
    let out_dir = std::env::var("OUT_DIR").unwrap();
    let out_dir = PathBuf::from(&out_dir);
    let original_cwd = std::env::current_dir().unwrap();
    std::fs::copy(&path, &out_dir.join("data.txt")).unwrap();
    std::env::set_current_dir(&out_dir).unwrap();
    std::fs::create_dir("nested").unwrap();
    symlink("data.txt", "relative_symlink.txt");
    symlink("../data.txt", "nested/relative_symlink.txt");
    std::env::set_current_dir(&original_cwd).unwrap();
}
