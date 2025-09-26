//! Tools for gathering various kinds of metadata (Cargo.lock, Cargo metadata, Crate Index info).

mod cargo_bin;
mod cargo_tree_resolver;
mod dependency;
mod metadata_annotation;
mod workspace_discoverer;

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use anyhow::{bail, Context, Result};
use camino::Utf8Path;
use cargo_lock::Lockfile as CargoLockfile;
use cargo_metadata::Metadata as CargoMetadata;
use tracing::debug;

pub(crate) use self::cargo_bin::*;
pub(crate) use self::cargo_tree_resolver::*;
pub(crate) use self::dependency::*;
pub(crate) use self::metadata_annotation::*;
pub(crate) use self::workspace_discoverer::*;

// TODO: This should also return a set of [crate-index::IndexConfig]s for packages in metadata.packages
/// A Trait for generating metadata (`cargo metadata` output and a lock file) from a Cargo manifest.
pub(crate) trait MetadataGenerator {
    fn generate<T: AsRef<Path>>(&self, manifest_path: T) -> Result<(CargoMetadata, CargoLockfile)>;
}

/// Generates Cargo metadata and a lockfile from a provided manifest.
pub(crate) struct Generator {
    /// The path to a `cargo` binary
    cargo_bin: Cargo,

    /// The path to a `rustc` binary
    rustc_bin: PathBuf,
}

impl Generator {
    pub(crate) fn new() -> Self {
        let rustc_bin = PathBuf::from(env::var("RUSTC").unwrap_or_else(|_| "rustc".to_string()));
        Generator {
            cargo_bin: Cargo::new(
                PathBuf::from(env::var("CARGO").unwrap_or_else(|_| "cargo".to_string())),
                rustc_bin.clone(),
            ),
            rustc_bin,
        }
    }

    pub(crate) fn with_cargo(mut self, cargo_bin: Cargo) -> Self {
        self.cargo_bin = cargo_bin;
        self
    }

    pub(crate) fn with_rustc(mut self, rustc_bin: PathBuf) -> Self {
        self.rustc_bin = rustc_bin;
        self
    }
}

impl MetadataGenerator for Generator {
    fn generate<T: AsRef<Path>>(&self, manifest_path: T) -> Result<(CargoMetadata, CargoLockfile)> {
        let manifest_dir = manifest_path
            .as_ref()
            .parent()
            .expect("The manifest should have a parent directory");
        let lockfile = {
            let lock_path = manifest_dir.join("Cargo.lock");
            if !lock_path.exists() {
                bail!("No `Cargo.lock` file was found with the given manifest")
            }
            cargo_lock::Lockfile::load(lock_path)?
        };

        let metadata = self
            .cargo_bin
            .metadata_command_with_options(manifest_path.as_ref(), vec!["--locked".to_owned()])?
            .exec()?;

        Ok((metadata, lockfile))
    }
}

/// A configuration describing how to invoke [cargo update](https://doc.rust-lang.org/cargo/commands/cargo-update.html).
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CargoUpdateRequest {
    /// Translates to an unrestricted `cargo update` command
    Eager,

    /// Translates to `cargo update --workspace`
    Workspace,

    /// Translates to `cargo update --package foo` with an optional `--precise` argument.
    Package {
        /// The name of the crate used with `--package`.
        name: String,

        /// If set, the `--precise` value that pairs with `--package`.
        version: Option<String>,
    },
}

impl FromStr for CargoUpdateRequest {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let lower = s.to_lowercase();

        if ["eager", "full", "all"].contains(&lower.as_str()) {
            return Ok(Self::Eager);
        }

        if ["1", "yes", "true", "on", "workspace", "minimal"].contains(&lower.as_str()) {
            return Ok(Self::Workspace);
        }

        let mut split = s.splitn(2, '=');
        Ok(Self::Package {
            name: split.next().map(|s| s.to_owned()).unwrap(),
            version: split.next().map(|s| s.to_owned()),
        })
    }
}

impl CargoUpdateRequest {
    /// Determine what arguments to pass to the `cargo update` command.
    fn get_update_args(&self) -> Vec<String> {
        match self {
            CargoUpdateRequest::Eager => Vec::new(),
            CargoUpdateRequest::Workspace => vec!["--workspace".to_owned()],
            CargoUpdateRequest::Package { name, version } => {
                let mut update_args = vec!["--package".to_owned(), name.clone()];

                if let Some(version) = version {
                    update_args.push("--precise".to_owned());
                    update_args.push(version.clone());
                }

                update_args
            }
        }
    }

    /// Calls `cargo update` with arguments specific to the state of the current variant.
    pub(crate) fn update(&self, manifest: &Path, cargo_bin: &Cargo) -> Result<()> {
        let manifest_dir = manifest.parent().unwrap();

        // Simply invoke `cargo update`
        let output = cargo_bin
            .command()?
            // Cargo detects config files based on `pwd` when running so
            // to ensure user provided Cargo config files are used, it's
            // critical to set the working directory to the manifest dir.
            .current_dir(manifest_dir)
            .arg("update")
            .arg("--manifest-path")
            .arg(manifest)
            .args(self.get_update_args())
            .output()
            .with_context(|| {
                format!(
                    "Error running cargo to update packages for manifest '{}'",
                    manifest.display()
                )
            })?;

        if !output.status.success() {
            eprintln!("{}", String::from_utf8_lossy(&output.stdout));
            eprintln!("{}", String::from_utf8_lossy(&output.stderr));
            bail!(format!("Failed to update lockfile: {}", output.status))
        }

        Ok(())
    }
}

pub(crate) struct LockGenerator {
    /// Interface to cargo.
    cargo_bin: Cargo,
}

impl LockGenerator {
    pub(crate) fn new(cargo_bin: Cargo) -> Self {
        Self { cargo_bin }
    }

    #[tracing::instrument(name = "LockGenerator::generate", skip_all)]
    pub(crate) fn generate(
        &self,
        manifest_path: &Utf8Path,
        existing_lock: &Option<PathBuf>,
        update_request: &Option<CargoUpdateRequest>,
    ) -> Result<cargo_lock::Lockfile> {
        debug!("Generating Cargo Lockfile for {}", manifest_path);

        let manifest_dir = manifest_path.parent().unwrap();
        let generated_lockfile_path = manifest_dir.join("Cargo.lock");

        if let Some(lock) = existing_lock {
            debug!("Using existing lock {}", lock.display());
            if !lock.exists() {
                bail!(
                    "An existing lockfile path was provided but a file at '{}' does not exist",
                    lock.display()
                )
            }

            // Install the file into the target location
            if generated_lockfile_path.exists() {
                fs::remove_file(&generated_lockfile_path)?;
            }
            fs::copy(lock, &generated_lockfile_path)?;

            if let Some(request) = update_request {
                request.update(manifest_path.as_std_path(), &self.cargo_bin)?;
            }

            // Ensure the Cargo cache is up to date to simulate the behavior
            // of having just generated a new one
            let output = self
                .cargo_bin
                .command()?
                // Cargo detects config files based on `pwd` when running so
                // to ensure user provided Cargo config files are used, it's
                // critical to set the working directory to the manifest dir.
                .current_dir(manifest_dir.as_std_path())
                .arg("fetch")
                .arg("--manifest-path")
                .arg(manifest_path.as_std_path())
                .output()
                .context(format!(
                    "Error running cargo to fetch crates '{}'",
                    manifest_path
                ))?;

            if !output.status.success() {
                eprintln!("{}", String::from_utf8_lossy(&output.stdout));
                eprintln!("{}", String::from_utf8_lossy(&output.stderr));
                bail!(format!(
                    "Failed to fetch crates for lockfile: {}",
                    output.status
                ))
            }
        } else {
            debug!("Generating new lockfile");
            // Simply invoke `cargo generate-lockfile`
            let output = self
                .cargo_bin
                .command()?
                // Cargo detects config files based on `pwd` when running so
                // to ensure user provided Cargo config files are used, it's
                // critical to set the working directory to the manifest dir.
                .current_dir(manifest_dir.as_std_path())
                .arg("generate-lockfile")
                .arg("--manifest-path")
                .arg(manifest_path.as_std_path())
                .output()
                .context(format!(
                    "Error running cargo to generate lockfile '{}'",
                    manifest_path
                ))?;

            if !output.status.success() {
                eprintln!("{}", String::from_utf8_lossy(&output.stdout));
                eprintln!("{}", String::from_utf8_lossy(&output.stderr));
                bail!(format!("Failed to generate lockfile: {}", output.status))
            }
        }

        cargo_lock::Lockfile::load(&generated_lockfile_path).context(format!(
            "Failed to load lockfile: {}",
            generated_lockfile_path
        ))
    }
}

/// A generator which runs `cargo vendor` on a given manifest
pub(crate) struct VendorGenerator {
    /// The path to a `cargo` binary
    cargo_bin: Cargo,

    /// The path to a `rustc` binary
    rustc_bin: PathBuf,
}

impl VendorGenerator {
    pub(crate) fn new(cargo_bin: Cargo, rustc_bin: PathBuf) -> Self {
        Self {
            cargo_bin,
            rustc_bin,
        }
    }
    #[tracing::instrument(name = "VendorGenerator::generate", skip_all)]
    pub(crate) fn generate(&self, manifest_path: &Utf8Path, output_dir: &Path) -> Result<()> {
        debug!("Vendoring {} to {}", manifest_path, output_dir.display());
        let manifest_dir = manifest_path.parent().unwrap();

        // Simply invoke `cargo generate-lockfile`
        let output = self
            .cargo_bin
            .command()?
            // Cargo detects config files based on `pwd` when running so
            // to ensure user provided Cargo config files are used, it's
            // critical to set the working directory to the manifest dir.
            .current_dir(manifest_dir.as_std_path())
            .arg("vendor")
            .arg("--manifest-path")
            .arg(manifest_path.as_std_path())
            .arg("--locked")
            .arg("--versioned-dirs")
            .arg(output_dir)
            .env("RUSTC", &self.rustc_bin)
            .output()
            .with_context(|| {
                format!(
                    "Error running cargo to vendor sources for manifest '{}'",
                    manifest_path
                )
            })?;

        if !output.status.success() {
            eprintln!("{}", String::from_utf8_lossy(&output.stdout));
            eprintln!("{}", String::from_utf8_lossy(&output.stderr));
            bail!(format!("Failed to vendor sources with: {}", output.status))
        }

        debug!("Done");
        Ok(())
    }
}

/// A helper function for writing Cargo metadata to a file.
pub(crate) fn write_metadata(path: &Path, metadata: &cargo_metadata::Metadata) -> Result<()> {
    let content =
        serde_json::to_string_pretty(metadata).context("Failed to serialize Cargo Metadata")?;

    fs::write(path, content).context("Failed to write metadata to disk")
}

/// A helper function for deserializing Cargo metadata and lockfiles
pub(crate) fn load_metadata(
    metadata_path: &Path,
) -> Result<(cargo_metadata::Metadata, cargo_lock::Lockfile)> {
    // Locate the Cargo.lock file related to the metadata file.
    let lockfile_path = metadata_path
        .parent()
        .expect("metadata files should always have parents")
        .join("Cargo.lock");
    if !lockfile_path.exists() {
        bail!(
            "The metadata file at {} is not next to a `Cargo.lock` file.",
            metadata_path.display()
        )
    }

    let content = fs::read_to_string(metadata_path)
        .with_context(|| format!("Failed to load Cargo Metadata: {}", metadata_path.display()))?;

    let metadata =
        serde_json::from_str(&content).context("Unable to deserialize Cargo metadata")?;

    let lockfile = cargo_lock::Lockfile::load(&lockfile_path)
        .with_context(|| format!("Failed to load lockfile: {}", lockfile_path.display()))?;

    Ok((metadata, lockfile))
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn deserialize_cargo_update_request_for_eager() {
        for value in ["all", "full", "eager"] {
            let request = CargoUpdateRequest::from_str(value).unwrap();

            assert_eq!(request, CargoUpdateRequest::Eager);
        }
    }

    #[test]
    fn deserialize_cargo_update_request_for_workspace() {
        for value in ["1", "true", "yes", "on", "workspace", "minimal"] {
            let request = CargoUpdateRequest::from_str(value).unwrap();

            assert_eq!(request, CargoUpdateRequest::Workspace);
        }
    }

    #[test]
    fn deserialize_cargo_update_request_for_package() {
        let request = CargoUpdateRequest::from_str("cargo-bazel").unwrap();

        assert_eq!(
            request,
            CargoUpdateRequest::Package {
                name: "cargo-bazel".to_owned(),
                version: None
            }
        );
    }

    #[test]
    fn deserialize_cargo_update_request_for_precise() {
        let request = CargoUpdateRequest::from_str("cargo-bazel@1.2.3").unwrap();

        assert_eq!(
            request,
            CargoUpdateRequest::Package {
                name: "cargo-bazel@1.2.3".to_owned(),
                version: None
            }
        );
    }

    #[test]
    fn deserialize_cargo_update_request_for_precise_pin() {
        let request = CargoUpdateRequest::from_str("cargo-bazel@1.2.3=4.5.6").unwrap();

        assert_eq!(
            request,
            CargoUpdateRequest::Package {
                name: "cargo-bazel@1.2.3".to_owned(),
                version: Some("4.5.6".to_owned()),
            }
        );
    }
}
