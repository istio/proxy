//! Tools for invoking Cargo subcommands.

use std::collections::BTreeMap;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::{Arc, Mutex};

use anyhow::{bail, Context, Result};
use cargo_metadata::MetadataCommand;
use semver::Version;

use crate::lockfile::Digest;

/// Cargo encapsulates a path to a `cargo` binary.
/// Any invocations of `cargo` (either as a `std::process::Command` or via `cargo_metadata`) should
/// go via this wrapper to ensure that any environment variables needed are set appropriately.
#[derive(Debug, Clone)]
pub(crate) struct Cargo {
    path: PathBuf,
    rustc_path: PathBuf,
    full_version: Arc<Mutex<Option<String>>>,
    cargo_home: Option<PathBuf>,
}

impl Cargo {
    pub(crate) fn new(path: PathBuf, rustc: PathBuf) -> Cargo {
        Cargo {
            path,
            rustc_path: rustc,
            full_version: Arc::new(Mutex::new(None)),
            cargo_home: None,
        }
    }

    /// Returns a new `Command` for running this cargo.
    pub(crate) fn command(&self) -> Result<Command> {
        let mut command = Command::new(&self.path);
        command.envs(self.env()?);
        if self.is_nightly()? {
            command.arg("-Zbindeps");
        }
        Ok(command)
    }

    /// Returns a new `MetadataCommand` using this cargo.
    /// `manifest_path`, `current_dir`, and `other_options` should not be called on the resturned MetadataCommand - instead pass them as the relevant args.
    pub(crate) fn metadata_command_with_options(
        &self,
        manifest_path: &Path,
        other_options: Vec<String>,
    ) -> Result<MetadataCommand> {
        let mut command = MetadataCommand::new();
        command.cargo_path(&self.path);
        for (k, v) in self.env()? {
            command.env(k, v);
        }

        command.manifest_path(manifest_path);
        // Cargo detects config files based on `pwd` when running so
        // to ensure user provided Cargo config files are used, it's
        // critical to set the working directory to the manifest dir.
        let manifest_dir = manifest_path
            .parent()
            .ok_or_else(|| anyhow::anyhow!("manifest_path {:?} must have parent", manifest_path))?;
        command.current_dir(manifest_dir);

        let mut other_options = other_options;
        if self.is_nightly()? {
            other_options.push("-Zbindeps".to_owned());
        }
        command.other_options(other_options);
        Ok(command)
    }

    /// Returns the output of running `cargo version`, trimming any leading or trailing whitespace.
    /// This function performs normalisation to work around `<https://github.com/rust-lang/cargo/issues/10547>`
    pub(crate) fn full_version(&self) -> Result<String> {
        let mut full_version = self.full_version.lock().unwrap();
        if full_version.is_none() {
            let observed_version = Digest::bin_version(&self.path)?;
            *full_version = Some(observed_version);
        }
        Ok(full_version.clone().unwrap())
    }

    pub(crate) fn is_nightly(&self) -> Result<bool> {
        let full_version = self.full_version()?;
        let version_str = full_version.split(' ').nth(1);
        if let Some(version_str) = version_str {
            let version = Version::parse(version_str).context("Failed to parse cargo version")?;
            return Ok(version.pre.as_str() == "nightly");
        }
        bail!("Couldn't parse cargo version");
    }

    pub(crate) fn use_sparse_registries_for_crates_io(&self) -> Result<bool> {
        let full_version = self.full_version()?;
        let version_str = full_version.split(' ').nth(1);
        if let Some(version_str) = version_str {
            let version = Version::parse(version_str).context("Failed to parse cargo version")?;
            return Ok(version.major >= 1 && version.minor >= 68);
        }
        bail!("Couldn't parse cargo version");
    }

    /// Determine if Cargo is expected to be using the new package_id spec. For
    /// details see <https://github.com/rust-lang/cargo/pull/13311>
    #[cfg(test)]
    pub(crate) fn uses_new_package_id_format(&self) -> Result<bool> {
        let full_version = self.full_version()?;
        let version_str = full_version.split(' ').nth(1);
        if let Some(version_str) = version_str {
            let version = Version::parse(version_str).context("Failed to parse cargo version")?;
            return Ok(version.major >= 1 && version.minor >= 77);
        }
        bail!("Couldn't parse cargo version");
    }

    /// Determine if Cargo is on a version which uses new hashing behavior
    /// introduced in Rust 1.86.0. For details see <https://github.com/frewsxcv/rust-crates-index/issues/182>
    pub(crate) fn uses_stable_registry_hash(&self) -> Result<bool> {
        let full_version = self.full_version()?;
        let version_str = full_version.split(' ').nth(1);
        if let Some(version_str) = version_str {
            let version = Version::parse(version_str).context("Failed to parse cargo version")?;
            return Ok(version.major >= 1 && version.minor >= 85);
        }
        bail!("Couldn't parse cargo version");
    }

    fn env(&self) -> Result<BTreeMap<String, OsString>> {
        let mut map = BTreeMap::new();

        map.insert("RUSTC".into(), self.rustc_path.as_os_str().to_owned());

        if self.use_sparse_registries_for_crates_io()? {
            map.insert(
                "CARGO_REGISTRIES_CRATES_IO_PROTOCOL".into(),
                "sparse".into(),
            );
        }

        if let Some(cargo_home) = &self.cargo_home {
            map.insert("CARGO_HOME".into(), cargo_home.as_os_str().to_owned());
        }

        Ok(map)
    }
}
