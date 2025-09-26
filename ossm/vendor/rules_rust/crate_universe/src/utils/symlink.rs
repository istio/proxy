use std::path::Path;

/// Create a symlink file on unix systems
#[cfg(target_family = "unix")]
pub(crate) fn symlink(src: &Path, dest: &Path) -> Result<(), std::io::Error> {
    std::os::unix::fs::symlink(src, dest)
}

/// Create a symlink file on windows systems
#[cfg(target_family = "windows")]
pub(crate) fn symlink(src: &Path, dest: &Path) -> Result<(), std::io::Error> {
    if src.is_dir() {
        std::os::windows::fs::symlink_dir(src, dest)
    } else {
        std::os::windows::fs::symlink_file(src, dest)
    }
}

/// Create a symlink file on unix systems
#[cfg(target_family = "unix")]
pub(crate) fn remove_symlink(path: &Path) -> Result<(), std::io::Error> {
    std::fs::remove_file(path)
}

/// Create a symlink file on windows systems
#[cfg(target_family = "windows")]
pub(crate) fn remove_symlink(path: &Path) -> Result<(), std::io::Error> {
    if path.is_dir() {
        std::fs::remove_dir(path)
    } else {
        std::fs::remove_file(path)
    }
}
