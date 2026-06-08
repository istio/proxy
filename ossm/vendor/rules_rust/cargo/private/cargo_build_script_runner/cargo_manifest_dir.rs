//! Bazel interactions with `CARGO_MANIFEST_DIR`.

use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

pub type RlocationPath = String;

/// Create a symlink file on unix systems
#[cfg(target_family = "unix")]
pub fn symlink(src: &Path, dest: &Path) -> Result<(), std::io::Error> {
    std::os::unix::fs::symlink(src, dest)
}

/// Create a symlink file on windows systems
#[cfg(target_family = "windows")]
pub fn symlink(src: &Path, dest: &Path) -> Result<(), std::io::Error> {
    if src.is_dir() {
        std::os::windows::fs::symlink_dir(src, dest)
    } else {
        std::os::windows::fs::symlink_file(src, dest)
    }
}

/// Create a symlink file on unix systems
#[cfg(target_family = "unix")]
pub fn remove_symlink(path: &Path) -> Result<(), std::io::Error> {
    std::fs::remove_file(path)
}

/// Create a symlink file on windows systems
#[cfg(target_family = "windows")]
pub fn remove_symlink(path: &Path) -> Result<(), std::io::Error> {
    if path.is_dir() {
        std::fs::remove_dir(path)
    } else {
        std::fs::remove_file(path)
    }
}

/// Check if the system supports symlinks by attempting to create one.
fn system_supports_symlinks(test_dir: &Path) -> Result<bool, String> {
    let test_file = test_dir.join("cbsr.txt");
    std::fs::write(&test_file, "").map_err(|e| {
        format!(
            "Failed to write test file for checking symlink support '{}' with {:?}",
            test_file.display(),
            e
        )
    })?;
    let test_link = test_dir.join("cbsr.link.txt");
    match symlink(&test_file, &test_link) {
        Err(_) => {
            std::fs::remove_file(test_file).map_err(|e| {
                format!("Failed to delete file {} with {:?}", test_link.display(), e)
            })?;
            Ok(false)
        }
        Ok(_) => {
            remove_symlink(&test_link).map_err(|e| {
                format!(
                    "Failed to remove symlink {} with {:?}",
                    test_link.display(),
                    e
                )
            })?;
            std::fs::remove_file(test_file).map_err(|e| {
                format!("Failed to delete file {} with {:?}", test_link.display(), e)
            })?;
            Ok(true)
        }
    }
}

fn is_dir_empty(path: &Path) -> Result<bool, String> {
    let mut entries = std::fs::read_dir(path)
        .map_err(|e| format!("Failed to read directory: {} with {:?}", path.display(), e))?;

    Ok(entries.next().is_none())
}

/// A struct for generating runfiles directories to use when running Cargo build scripts.
pub struct RunfilesMaker {
    /// The output where a runfiles-like directory should be written.
    output_dir: PathBuf,

    /// A list of file suffixes to retain when pruning runfiles.
    filename_suffixes_to_retain: BTreeSet<String>,

    /// Runfiles to include in `output_dir`.
    runfiles: BTreeMap<PathBuf, RlocationPath>,
}

impl RunfilesMaker {
    pub fn from_param_file(arg: &str) -> RunfilesMaker {
        assert!(
            arg.starts_with('@'),
            "Expected arg to be a params file. Got {}",
            arg
        );

        let content = std::fs::read_to_string(
            arg.strip_prefix('@')
                .expect("Param files should start with @"),
        )
        .unwrap();
        let mut args = content.lines();

        let output_dir = PathBuf::from(
            args.next()
                .unwrap_or_else(|| panic!("Not enough arguments provided.")),
        );
        let filename_suffixes_to_retain = args
            .next()
            .unwrap_or_else(|| panic!("Not enough arguments provided."))
            .split(',')
            .map(|s| s.to_owned())
            .collect::<BTreeSet<String>>();
        let runfiles = args
            .map(|s| {
                let s = if s.starts_with('\'') && s.ends_with('\'') {
                    s.trim_matches('\'')
                } else {
                    s
                };
                let (src, dest) = s
                    .split_once('=')
                    .unwrap_or_else(|| panic!("Unexpected runfiles argument: {}", s));
                (PathBuf::from(src), RlocationPath::from(dest))
            })
            .collect::<BTreeMap<_, _>>();

        assert!(!runfiles.is_empty(), "No runfiles found");

        RunfilesMaker {
            output_dir,
            filename_suffixes_to_retain,
            runfiles,
        }
    }

    /// Create a runfiles directory.
    #[cfg(target_family = "unix")]
    pub fn create_runfiles_dir(&self) -> Result<(), String> {
        for (src, dest) in &self.runfiles {
            let abs_dest = self.output_dir.join(dest);

            if let Some(parent) = abs_dest.parent() {
                if !parent.exists() {
                    std::fs::create_dir_all(parent).map_err(|e| {
                        format!(
                            "Failed to create parent directory '{}' for '{}' with {:?}",
                            parent.display(),
                            abs_dest.display(),
                            e
                        )
                    })?;
                }
            }

            let abs_src = std::env::current_dir().unwrap().join(src);

            symlink(&abs_src, &abs_dest).map_err(|e| {
                format!(
                    "Failed to link `{} -> {}` with {:?}",
                    abs_src.display(),
                    abs_dest.display(),
                    e
                )
            })?;
        }

        Ok(())
    }

    /// Create a runfiles directory.
    #[cfg(target_family = "windows")]
    pub fn create_runfiles_dir(&self) -> Result<(), String> {
        if !self.output_dir.exists() {
            std::fs::create_dir_all(&self.output_dir).map_err(|e| {
                format!(
                    "Failed to create output directory '{}' with {:?}",
                    self.output_dir.display(),
                    e
                )
            })?;
        }

        let supports_symlinks = system_supports_symlinks(&self.output_dir)?;

        for (src, dest) in &self.runfiles {
            let abs_dest = self.output_dir.join(dest);
            if let Some(parent) = abs_dest.parent() {
                if !parent.exists() {
                    std::fs::create_dir_all(parent).map_err(|e| {
                        format!(
                            "Failed to create parent directory '{}' for '{}' with {:?}",
                            parent.display(),
                            abs_dest.display(),
                            e
                        )
                    })?;
                }
            }

            if supports_symlinks {
                let abs_src = std::env::current_dir().unwrap().join(src);

                symlink(&abs_src, &abs_dest).map_err(|e| {
                    format!(
                        "Failed to link `{} -> {}` with {:?}",
                        abs_src.display(),
                        abs_dest.display(),
                        e
                    )
                })?;
            } else {
                std::fs::copy(src, &abs_dest).map_err(|e| {
                    format!(
                        "Failed to copy `{} -> {}` with {:?}",
                        src.display(),
                        abs_dest.display(),
                        e
                    )
                })?;
            }
        }
        Ok(())
    }

    /// Delete runfiles from the runfiles directory that do not match user defined suffixes
    ///
    /// The Unix implementation assumes symlinks are supported and that the runfiles directory
    /// was created using symlinks.
    fn drain_runfiles_dir_unix(&self) -> Result<(), String> {
        for (src, dest) in &self.runfiles {
            let abs_dest = self.output_dir.join(dest);

            remove_symlink(&abs_dest).map_err(|e| {
                format!(
                    "Failed to delete symlink '{}' with {:?}",
                    abs_dest.display(),
                    e
                )
            })?;

            if !self
                .filename_suffixes_to_retain
                .iter()
                .any(|suffix| dest.ends_with(suffix))
            {
                if let Some(parent) = abs_dest.parent() {
                    if is_dir_empty(parent).map_err(|e| {
                        format!("Failed to determine if directory was empty with: {:?}", e)
                    })? {
                        std::fs::remove_dir(parent).map_err(|e| {
                            format!(
                                "Failed to delete directory {} with {:?}",
                                parent.display(),
                                e
                            )
                        })?;
                    }
                }
                continue;
            }

            std::fs::copy(src, &abs_dest).map_err(|e| {
                format!(
                    "Failed to copy `{} -> {}` with {:?}",
                    src.display(),
                    abs_dest.display(),
                    e
                )
            })?;
        }
        Ok(())
    }

    /// Delete runfiles from the runfiles directory that do not match user defined suffixes
    ///
    /// The Windows implementation assumes symlinks are not supported and real files will have
    /// been copied into the runfiles directory.
    fn drain_runfiles_dir_windows(&self) -> Result<(), String> {
        for dest in self.runfiles.values() {
            if !self
                .filename_suffixes_to_retain
                .iter()
                .any(|suffix| dest.ends_with(suffix))
            {
                continue;
            }

            let abs_dest = self.output_dir.join(dest);
            std::fs::remove_file(&abs_dest).map_err(|e| {
                format!("Failed to remove file {} with {:?}", abs_dest.display(), e)
            })?;
        }
        Ok(())
    }

    /// Delete runfiles from the runfiles directory that do not match user defined suffixes
    pub fn drain_runfiles_dir(&self, out_dir: &Path) -> Result<(), String> {
        if cfg!(target_family = "windows") {
            // If symlinks are supported then symlinks will have been used.
            let supports_symlinks = system_supports_symlinks(&self.output_dir)?;
            if supports_symlinks {
                self.drain_runfiles_dir_unix()?;
            } else {
                self.drain_runfiles_dir_windows()?;
            }
        } else {
            self.drain_runfiles_dir_unix()?;
        }

        // Due to the symlinks in `CARGO_MANIFEST_DIR`, some build scripts
        // may have placed symlinks over real files in `OUT_DIR`. To counter
        // this, all non-relative symlinks are resolved.
        replace_symlinks_in_out_dir(out_dir)
    }
}

/// Iterates over the given directory recursively and resolves any symlinks
///
/// Symlinks shouldn't present in `out_dir` as those amy contain paths to sandboxes which doesn't exists anymore.
/// Therefore, bazel will fail because of dangling symlinks.
fn replace_symlinks_in_out_dir(out_dir: &Path) -> Result<(), String> {
    if out_dir.is_dir() {
        let out_dir_paths = std::fs::read_dir(out_dir).map_err(|e| {
            format!(
                "Failed to read directory `{}` with {:?}",
                out_dir.display(),
                e
            )
        })?;
        for entry in out_dir_paths {
            let entry =
                entry.map_err(|e| format!("Failed to read directory entry with  {:?}", e,))?;
            let path = entry.path();

            if path.is_symlink() {
                let target_path = std::fs::read_link(&path).map_err(|e| {
                    format!("Failed to read symlink `{}` with {:?}", path.display(), e,)
                })?;
                // we don't want to replace relative symlinks
                if target_path.is_relative() {
                    continue;
                }
                std::fs::remove_file(&path)
                    .map_err(|e| format!("Failed remove file `{}` with {:?}", path.display(), e))?;
                std::fs::copy(&target_path, &path).map_err(|e| {
                    format!(
                        "Failed to copy `{} -> {}` with {:?}",
                        target_path.display(),
                        path.display(),
                        e
                    )
                })?;
            } else if path.is_dir() {
                replace_symlinks_in_out_dir(&path).map_err(|e| {
                    format!(
                        "Failed to normalize nested directory `{}` with {}",
                        path.display(),
                        e,
                    )
                })?;
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {

    use std::fs;
    use std::io::Write;

    use super::*;

    fn prepare_output_dir_with_symlinks() -> PathBuf {
        let test_tmp = PathBuf::from(std::env::var("TEST_TMPDIR").unwrap());
        let out_dir = test_tmp.join("out_dir");
        fs::create_dir(&out_dir).unwrap();
        let nested_dir = out_dir.join("nested");
        fs::create_dir(nested_dir).unwrap();

        let temp_dir_file = test_tmp.join("outside.txt");
        let mut file = fs::File::create(&temp_dir_file).unwrap();
        file.write_all(b"outside world").unwrap();
        // symlink abs path outside of the out_dir
        symlink(&temp_dir_file, &out_dir.join("outside.txt")).unwrap();

        let inside_dir_file = out_dir.join("inside.txt");
        let mut file = fs::File::create(inside_dir_file).unwrap();
        file.write_all(b"inside world").unwrap();
        // symlink relative next to the file in the out_dir
        symlink(
            &PathBuf::from("inside.txt"),
            &out_dir.join("inside_link.txt"),
        )
        .unwrap();
        // symlink relative within a subdir in the out_dir
        symlink(
            &PathBuf::from("..").join("inside.txt"),
            &out_dir.join("nested").join("inside_link.txt"),
        )
        .unwrap();

        out_dir
    }

    #[cfg(any(target_family = "windows", target_family = "unix"))]
    #[test]
    fn replace_symlinks_in_out_dir() {
        let out_dir = prepare_output_dir_with_symlinks();
        super::replace_symlinks_in_out_dir(&out_dir).unwrap();

        // this should be replaced because it is an absolute symlink
        let file_path = out_dir.join("outside.txt");
        assert!(!file_path.is_symlink());
        let contents = fs::read_to_string(file_path).unwrap();
        assert_eq!(contents, "outside world");

        // this is the file created inside the out_dir
        let file_path = out_dir.join("inside.txt");
        assert!(!file_path.is_symlink());
        let contents = fs::read_to_string(file_path).unwrap();
        assert_eq!(contents, "inside world");

        // this is the symlink in the out_dir
        let file_path = out_dir.join("inside_link.txt");
        assert!(file_path.is_symlink());
        let contents = fs::read_to_string(file_path).unwrap();
        assert_eq!(contents, "inside world");

        // this is the symlink in the out_dir under another directory which refers to ../inside.txt
        let file_path = out_dir.join("nested").join("inside_link.txt");
        assert!(file_path.is_symlink());
        let contents = fs::read_to_string(file_path).unwrap();
        assert_eq!(contents, "inside world");
    }
}
