//! Runfiles lookup library for Bazel-built Rust binaries and tests.
//!
//! USAGE:
//!
//! 1. Depend on this runfiles library from your build rule:
//! ```python
//!   rust_binary(
//!       name = "my_binary",
//!       ...
//!       data = ["//path/to/my/data.txt"],
//!       deps = ["@rules_rust//tools/runfiles"],
//!   )
//! ```
//!
//! 2. Import the runfiles library.
//! ```ignore
//! use runfiles::Runfiles;
//! ```
//!
//! 3. Create a Runfiles object and use `rlocation!`` to look up runfile paths:
//! ```ignore
//!
//! use runfiles::{Runfiles, rlocation};
//!
//! let r = Runfiles::create().unwrap();
//! let path = rlocation!(r, "my_workspace/path/to/my/data.txt").expect("Failed to locate runfile");
//!
//! let f = File::open(path).unwrap();
//!
//! // ...
//! ```

use std::collections::HashMap;
use std::env;
use std::fs;
use std::io;
use std::path::Path;
use std::path::PathBuf;

const RUNFILES_DIR_ENV_VAR: &str = "RUNFILES_DIR";
const MANIFEST_FILE_ENV_VAR: &str = "RUNFILES_MANIFEST_FILE";
const TEST_SRCDIR_ENV_VAR: &str = "TEST_SRCDIR";

#[macro_export]
macro_rules! rlocation {
    ($r:expr, $path:expr) => {
        $r.rlocation_from($path, env!("REPOSITORY_NAME"))
    };
}

/// The error type for [Runfiles] construction.
#[derive(Debug)]
pub enum RunfilesError {
    /// Directory based runfiles could not be found.
    RunfilesDirNotFound,

    /// An [I/O Error](https://doc.rust-lang.org/std/io/struct.Error.html)
    /// which occurred during the creation of directory-based runfiles.
    RunfilesDirIoError(io::Error),

    /// An [I/O Error](https://doc.rust-lang.org/std/io/struct.Error.html)
    /// which occurred during the creation of manifest-file-based runfiles.
    RunfilesManifestIoError(io::Error),

    /// A manifest file could not be parsed.
    RunfilesManifestInvalidFormat,

    /// The bzlmod repo-mapping file could not be found.
    RepoMappingNotFound,

    /// The bzlmod repo-mapping file could not be parsed.
    RepoMappingInvalidFormat,

    /// An [I/O Error](https://doc.rust-lang.org/std/io/struct.Error.html)
    /// which occurred during the parsing of a repo-mapping file.
    RepoMappingIoError(io::Error),

    /// An error indicating a specific Runfile was not found.
    RunfileNotFound(PathBuf),

    /// An [I/O Error](https://doc.rust-lang.org/std/io/struct.Error.html)
    /// which occurred when operating with a particular runfile.
    RunfileIoError(io::Error),
}

impl std::fmt::Display for RunfilesError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            RunfilesError::RunfilesDirNotFound => write!(f, "RunfilesDirNotFound"),
            RunfilesError::RunfilesDirIoError(err) => write!(f, "RunfilesDirIoError: {:?}", err),
            RunfilesError::RunfilesManifestIoError(err) => {
                write!(f, "RunfilesManifestIoError: {:?}", err)
            }
            RunfilesError::RunfilesManifestInvalidFormat => write!(f, "RepoMappingInvalidFormat"),
            RunfilesError::RepoMappingNotFound => write!(f, "RepoMappingInvalidFormat"),
            RunfilesError::RepoMappingInvalidFormat => write!(f, "RepoMappingInvalidFormat"),
            RunfilesError::RepoMappingIoError(err) => write!(f, "RepoMappingIoError: {:?}", err),
            RunfilesError::RunfileNotFound(path) => {
                write!(f, "RunfileNotFound: {}", path.display())
            }
            RunfilesError::RunfileIoError(err) => write!(f, "RunfileIoError: {:?}", err),
        }
    }
}

impl std::error::Error for RunfilesError {}

impl PartialEq for RunfilesError {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::RunfilesDirIoError(l0), Self::RunfilesDirIoError(r0)) => {
                l0.to_string() == r0.to_string()
            }
            (Self::RunfilesManifestIoError(l0), Self::RunfilesManifestIoError(r0)) => {
                l0.to_string() == r0.to_string()
            }
            (Self::RepoMappingIoError(l0), Self::RepoMappingIoError(r0)) => {
                l0.to_string() == r0.to_string()
            }
            (Self::RunfileNotFound(l0), Self::RunfileNotFound(r0)) => l0 == r0,
            (Self::RunfileIoError(l0), Self::RunfileIoError(r0)) => {
                l0.to_string() == r0.to_string()
            }
            _ => core::mem::discriminant(self) == core::mem::discriminant(other),
        }
    }
}

/// A specialized [`std::result::Result`] type for
pub type Result<T> = std::result::Result<T, RunfilesError>;

#[derive(Debug)]
enum Mode {
    /// Runfiles located in a directory indicated by the `RUNFILES_DIR` environment
    /// variable or a neighboring `*.runfiles` directory to the executable.
    DirectoryBased(PathBuf),

    /// Runfiles represented as a mapping of `rlocationpath` to real paths indicated
    /// by the `RUNFILES_MANIFEST_FILE` environment variable.
    ManifestBased(HashMap<PathBuf, PathBuf>),
}

type RepoMappingKey = (String, String);
type RepoMapping = HashMap<RepoMappingKey, String>;

/// An interface for accessing to [Bazel runfiles](https://bazel.build/extending/rules#runfiles).
#[derive(Debug)]
pub struct Runfiles {
    mode: Mode,
    repo_mapping: RepoMapping,
}

impl Runfiles {
    /// Creates a manifest based Runfiles object when
    /// RUNFILES_MANIFEST_FILE environment variable is present,
    /// or a directory based Runfiles object otherwise.
    pub fn create() -> Result<Self> {
        let mode = if let Some(manifest_file) = std::env::var_os(MANIFEST_FILE_ENV_VAR) {
            Self::create_manifest_based(Path::new(&manifest_file))?
        } else {
            Mode::DirectoryBased(find_runfiles_dir()?)
        };

        let repo_mapping = raw_rlocation(&mode, "_repo_mapping")
            // This is the only place directory based runfiles might do file IO for a runfile. In the
            // event that a `_repo_mapping` file does not exist, a default map should be created. Otherwise
            // if the file is known to exist, parse it and raise errors for users should parsing fail.
            .filter(|f| f.exists())
            .map(parse_repo_mapping)
            .transpose()?
            .unwrap_or_default();

        Ok(Runfiles { mode, repo_mapping })
    }

    fn create_manifest_based(manifest_path: &Path) -> Result<Mode> {
        let manifest_content = std::fs::read_to_string(manifest_path)
            .map_err(RunfilesError::RunfilesManifestIoError)?;
        let path_mapping = manifest_content
            .lines()
            .flat_map(|line| {
                let pair = line
                    .split_once(' ')
                    .ok_or(RunfilesError::RunfilesManifestInvalidFormat)?;
                Ok::<(PathBuf, PathBuf), RunfilesError>((pair.0.into(), pair.1.into()))
            })
            .collect::<HashMap<_, _>>();
        Ok(Mode::ManifestBased(path_mapping))
    }

    /// Returns the runtime path of a runfile.
    ///
    /// Runfiles are data-dependencies of Bazel-built binaries and tests.
    /// The returned path may not be valid. The caller should check the path's
    /// validity and that the path exists.
    /// @deprecated - this is not bzlmod-aware. Prefer the `rlocation!` macro or `rlocation_from`
    pub fn rlocation(&self, path: impl AsRef<Path>) -> Option<PathBuf> {
        let path = path.as_ref();
        if path.is_absolute() {
            return Some(path.to_path_buf());
        }
        raw_rlocation(&self.mode, path)
    }

    /// Returns the runtime path of a runfile.
    ///
    /// Runfiles are data-dependencies of Bazel-built binaries and tests.
    /// The returned path may not be valid. The caller should check the path's
    /// validity and that the path exists.
    ///
    /// Typically this should be used via the `rlocation!` macro to properly set source_repo.
    pub fn rlocation_from(&self, path: impl AsRef<Path>, source_repo: &str) -> Option<PathBuf> {
        let path = path.as_ref();
        if path.is_absolute() {
            return Some(path.to_path_buf());
        }

        let path_str = path.to_str().expect("Should be valid UTF8");
        let (repo_alias, repo_path): (&str, Option<&str>) = match path_str.split_once('/') {
            Some((name, alias)) => (name, Some(alias)),
            None => (path_str, None),
        };
        let key: (String, String) = (source_repo.into(), repo_alias.into());
        if let Some(target_repo_directory) = self.repo_mapping.get(&key) {
            match repo_path {
                Some(repo_path) => {
                    raw_rlocation(&self.mode, format!("{target_repo_directory}/{repo_path}"))
                }
                None => raw_rlocation(&self.mode, target_repo_directory),
            }
        } else {
            raw_rlocation(&self.mode, path)
        }
    }
}

fn raw_rlocation(mode: &Mode, path: impl AsRef<Path>) -> Option<PathBuf> {
    let path = path.as_ref();
    match mode {
        Mode::DirectoryBased(runfiles_dir) => Some(runfiles_dir.join(path)),
        Mode::ManifestBased(path_mapping) => path_mapping.get(path).cloned(),
    }
}

fn parse_repo_mapping(path: PathBuf) -> Result<RepoMapping> {
    let mut repo_mapping = RepoMapping::new();

    for line in std::fs::read_to_string(path)
        .map_err(RunfilesError::RepoMappingIoError)?
        .lines()
    {
        let parts: Vec<&str> = line.splitn(3, ',').collect();
        if parts.len() < 3 {
            return Err(RunfilesError::RepoMappingInvalidFormat);
        }
        repo_mapping.insert((parts[0].into(), parts[1].into()), parts[2].into());
    }

    Ok(repo_mapping)
}

/// Returns the .runfiles directory for the currently executing binary.
pub fn find_runfiles_dir() -> Result<PathBuf> {
    assert!(
        std::env::var_os(MANIFEST_FILE_ENV_VAR).is_none(),
        "Unexpected call when {} exists",
        MANIFEST_FILE_ENV_VAR
    );

    // If Bazel told us about the runfiles dir, use that without looking further.
    if let Some(runfiles_dir) = std::env::var_os(RUNFILES_DIR_ENV_VAR).map(PathBuf::from) {
        if runfiles_dir.is_dir() {
            return Ok(runfiles_dir);
        }
    }
    if let Some(test_srcdir) = std::env::var_os(TEST_SRCDIR_ENV_VAR).map(PathBuf::from) {
        if test_srcdir.is_dir() {
            return Ok(test_srcdir);
        }
    }

    // Consume the first argument (argv[0])
    let exec_path = std::env::args().next().expect("arg 0 was not set");

    let current_dir =
        env::current_dir().expect("The current working directory is always expected to be set.");

    let mut binary_path = PathBuf::from(&exec_path);
    loop {
        // Check for our neighboring `${binary}.runfiles` directory.
        let mut runfiles_name = binary_path.file_name().unwrap().to_owned();
        runfiles_name.push(".runfiles");

        let runfiles_path = binary_path.with_file_name(&runfiles_name);
        if runfiles_path.is_dir() {
            return Ok(runfiles_path);
        }

        // Check if we're already under a `*.runfiles` directory.
        {
            // TODO: 1.28 adds Path::ancestors() which is a little simpler.
            let mut next = binary_path.parent();
            while let Some(ancestor) = next {
                if ancestor
                    .file_name()
                    .is_some_and(|f| f.to_string_lossy().ends_with(".runfiles"))
                {
                    return Ok(ancestor.to_path_buf());
                }
                next = ancestor.parent();
            }
        }

        if !fs::symlink_metadata(&binary_path)
            .map_err(RunfilesError::RunfilesDirIoError)?
            .file_type()
            .is_symlink()
        {
            break;
        }
        // Follow symlinks and keep looking.
        let link_target = binary_path
            .read_link()
            .map_err(RunfilesError::RunfilesDirIoError)?;
        binary_path = if link_target.is_absolute() {
            link_target
        } else {
            let link_dir = binary_path.parent().unwrap();
            current_dir.join(link_dir).join(link_target)
        }
    }

    Err(RunfilesError::RunfilesDirNotFound)
}

#[cfg(test)]
mod test {
    use super::*;

    use std::ffi::OsStr;
    use std::fs::File;
    use std::io::prelude::*;

    /// Only `RUNFILES_DIR`` is set.
    ///
    /// This test is a part of `test_manifest_based_can_read_data_from_runfiles` as
    /// it modifies environment variables.
    fn test_env_only_runfiles_dir(test_srcdir: &OsStr, runfiles_manifest_file: &OsStr) {
        env::remove_var(TEST_SRCDIR_ENV_VAR);
        env::remove_var(MANIFEST_FILE_ENV_VAR);
        let r = Runfiles::create().unwrap();

        let d = rlocation!(r, "rules_rust").unwrap();
        let f = rlocation!(r, "rules_rust/tools/runfiles/data/sample.txt").unwrap();
        assert_eq!(d.join("tools/runfiles/data/sample.txt"), f);

        let mut f = File::open(f).unwrap();

        let mut buffer = String::new();
        f.read_to_string(&mut buffer).unwrap();

        assert_eq!("Example Text!", buffer);
        env::set_var(TEST_SRCDIR_ENV_VAR, test_srcdir);
        env::set_var(MANIFEST_FILE_ENV_VAR, runfiles_manifest_file);
    }

    /// Only `TEST_SRCDIR` is set.
    ///
    /// This test is a part of `test_manifest_based_can_read_data_from_runfiles` as
    /// it modifies environment variables.
    fn test_env_only_test_srcdir(runfiles_dir: &OsStr, runfiles_manifest_file: &OsStr) {
        env::remove_var(RUNFILES_DIR_ENV_VAR);
        env::remove_var(MANIFEST_FILE_ENV_VAR);
        let r = Runfiles::create().unwrap();

        let mut f = File::open(rlocation!(r, "rules_rust/tools/runfiles/data/sample.txt").unwrap())
            .unwrap();

        let mut buffer = String::new();
        f.read_to_string(&mut buffer).unwrap();

        assert_eq!("Example Text!", buffer);
        env::set_var(RUNFILES_DIR_ENV_VAR, runfiles_dir);
        env::set_var(MANIFEST_FILE_ENV_VAR, runfiles_manifest_file);
    }

    /// Neither `RUNFILES_DIR` or `TEST_SRCDIR` are set
    ///
    /// This test is a part of `test_manifest_based_can_read_data_from_runfiles` as
    /// it modifies environment variables.
    fn test_env_nothing_set(
        test_srcdir: &OsStr,
        runfiles_dir: &OsStr,
        runfiles_manifest_file: &OsStr,
    ) {
        env::remove_var(RUNFILES_DIR_ENV_VAR);
        env::remove_var(TEST_SRCDIR_ENV_VAR);
        env::remove_var(MANIFEST_FILE_ENV_VAR);

        let r = Runfiles::create().unwrap();

        let mut f = File::open(rlocation!(r, "rules_rust/tools/runfiles/data/sample.txt").unwrap())
            .unwrap();

        let mut buffer = String::new();
        f.read_to_string(&mut buffer).unwrap();

        assert_eq!("Example Text!", buffer);

        env::set_var(TEST_SRCDIR_ENV_VAR, test_srcdir);
        env::set_var(RUNFILES_DIR_ENV_VAR, runfiles_dir);
        env::set_var(MANIFEST_FILE_ENV_VAR, runfiles_manifest_file);
    }

    #[test]
    fn test_can_read_data_from_runfiles() {
        // We want to run multiple test cases with different environment variables set. Since
        // environment variables are global state, we need to ensure the test cases do not run
        // concurrently. Rust runs tests in parallel and does not provide an easy way to synchronise
        // them, so we run all test cases in the same #[test] function.

        let test_srcdir =
            env::var_os(TEST_SRCDIR_ENV_VAR).expect("bazel did not provide TEST_SRCDIR");
        let runfiles_dir =
            env::var_os(RUNFILES_DIR_ENV_VAR).expect("bazel did not provide RUNFILES_DIR");
        let runfiles_manifest_file = env::var_os(MANIFEST_FILE_ENV_VAR).unwrap_or("".into());

        test_env_only_runfiles_dir(&test_srcdir, &runfiles_manifest_file);
        test_env_only_test_srcdir(&runfiles_dir, &runfiles_manifest_file);
        test_env_nothing_set(&test_srcdir, &runfiles_dir, &runfiles_manifest_file);
    }

    #[test]
    fn test_manifest_based_can_read_data_from_runfiles() {
        let mut path_mapping = HashMap::new();
        path_mapping.insert("a/b".into(), "c/d".into());
        let r = Runfiles {
            mode: Mode::ManifestBased(path_mapping),
            repo_mapping: RepoMapping::new(),
        };

        assert_eq!(r.rlocation("a/b"), Some(PathBuf::from("c/d")));
    }

    #[test]
    fn test_manifest_based_missing_file() {
        let mut path_mapping = HashMap::new();
        path_mapping.insert("a/b".into(), "c/d".into());
        let r = Runfiles {
            mode: Mode::ManifestBased(path_mapping),
            repo_mapping: RepoMapping::new(),
        };

        assert_eq!(r.rlocation("does/not/exist"), None);
    }

    fn dedent(text: &str) -> String {
        text.lines()
            .map(|l| l.trim_start())
            .collect::<Vec<&str>>()
            .join("\n")
    }

    #[test]
    fn test_parse_repo_mapping() {
        let temp_dir = PathBuf::from(std::env::var("TEST_TMPDIR").unwrap());
        std::fs::create_dir_all(&temp_dir).unwrap();

        let valid = temp_dir.join("test_parse_repo_mapping.txt");
        std::fs::write(
            &valid,
            dedent(
                r#",rules_rust,rules_rust
            bazel_tools,__main__,rules_rust
            local_config_cc,rules_rust,rules_rust
            local_config_sh,rules_rust,rules_rust
            local_config_xcode,rules_rust,rules_rust
            platforms,rules_rust,rules_rust
            rules_rust_tinyjson,rules_rust,rules_rust
            rust_darwin_aarch64__aarch64-apple-darwin__stable_tools,rules_rust,rules_rust
            "#,
            ),
        )
        .unwrap();

        assert_eq!(
            parse_repo_mapping(valid),
            Ok(RepoMapping::from([
                (
                    ("local_config_xcode".to_owned(), "rules_rust".to_owned()),
                    "rules_rust".to_owned()
                ),
                (
                    ("platforms".to_owned(), "rules_rust".to_owned()),
                    "rules_rust".to_owned()
                ),
                (
                    (
                        "rust_darwin_aarch64__aarch64-apple-darwin__stable_tools".to_owned(),
                        "rules_rust".to_owned()
                    ),
                    "rules_rust".to_owned()
                ),
                (
                    ("rules_rust_tinyjson".to_owned(), "rules_rust".to_owned()),
                    "rules_rust".to_owned()
                ),
                (
                    ("local_config_sh".to_owned(), "rules_rust".to_owned()),
                    "rules_rust".to_owned()
                ),
                (
                    ("bazel_tools".to_owned(), "__main__".to_owned()),
                    "rules_rust".to_owned()
                ),
                (
                    ("local_config_cc".to_owned(), "rules_rust".to_owned()),
                    "rules_rust".to_owned()
                ),
                (
                    ("".to_owned(), "rules_rust".to_owned()),
                    "rules_rust".to_owned()
                )
            ]))
        );
    }

    #[test]
    fn test_parse_repo_mapping_invalid_file() {
        let temp_dir = PathBuf::from(std::env::var("TEST_TMPDIR").unwrap());
        std::fs::create_dir_all(&temp_dir).unwrap();

        let invalid = temp_dir.join("test_parse_repo_mapping_invalid_file.txt");

        assert!(matches!(
            parse_repo_mapping(invalid.clone()).err().unwrap(),
            RunfilesError::RepoMappingIoError(_)
        ));

        std::fs::write(&invalid, "invalid").unwrap();

        assert_eq!(
            parse_repo_mapping(invalid),
            Err(RunfilesError::RepoMappingInvalidFormat),
        );
    }
}
