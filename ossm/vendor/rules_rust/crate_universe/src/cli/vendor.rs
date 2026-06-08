//! The cli entrypoint for the `vendor` subcommand

use std::collections::{BTreeSet, HashMap};
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{self, ExitStatus};
use std::sync::Arc;

use anyhow::{anyhow, bail, Context as AnyhowContext};
use camino::Utf8PathBuf;
use clap::Parser;

use crate::config::{Config, VendorMode};
use crate::context::Context;
use crate::lockfile::{lock_context, write_lockfile};
use crate::metadata::CargoUpdateRequest;
use crate::metadata::TreeResolver;
use crate::metadata::{Annotations, Cargo, VendorGenerator};
use crate::rendering::{render_module_label, write_outputs, Renderer};
use crate::splicing::{generate_lockfile, Splicer, SplicingManifest, WorkspaceMetadata};
use crate::utils::normalize_cargo_file_paths;

/// Command line options for the `vendor` subcommand
#[derive(Parser, Debug)]
#[clap(about = "Command line options for the `vendor` subcommand", version)]
pub struct VendorOptions {
    /// The path to a Cargo binary to use for gathering metadata
    #[clap(long, env = "CARGO")]
    pub cargo: PathBuf,

    /// The path to a rustc binary for use with Cargo
    #[clap(long, env = "RUSTC")]
    pub rustc: PathBuf,

    /// The path to a buildifier binary for formatting generated BUILD files
    #[clap(long)]
    pub buildifier: Option<PathBuf>,

    /// The config file with information about the Bazel and Cargo workspace
    #[clap(long)]
    pub config: PathBuf,

    /// A generated manifest of splicing inputs
    #[clap(long)]
    pub splicing_manifest: PathBuf,

    /// The path to write a Bazel lockfile
    #[clap(long)]
    pub lockfile: Option<PathBuf>,

    /// The path to a [Cargo.lock](https://doc.rust-lang.org/cargo/guide/cargo-toml-vs-cargo-lock.html) file.
    #[clap(long)]
    pub cargo_lockfile: Option<PathBuf>,

    /// A [Cargo config](https://doc.rust-lang.org/cargo/reference/config.html#configuration)
    /// file to use when gathering metadata
    #[clap(long)]
    pub cargo_config: Option<PathBuf>,

    /// The desired update/repin behavior. The arguments passed here are forward to
    /// [cargo update](https://doc.rust-lang.org/cargo/commands/cargo-update.html). See
    /// [crate::metadata::CargoUpdateRequest] for details on the values to pass here.
    #[clap(long, env = "CARGO_BAZEL_REPIN", num_args=0..=1, default_missing_value = "true")]
    pub repin: Option<CargoUpdateRequest>,

    /// The path to a Cargo metadata `json` file.
    #[clap(long)]
    pub metadata: Option<PathBuf>,

    /// The path to a bazel binary
    #[clap(long, env = "BAZEL_REAL", default_value = "bazel")]
    pub bazel: PathBuf,

    /// The directory in which to build the workspace. A `Cargo.toml` file
    /// should always be produced within this directory.
    #[clap(long, env = "BUILD_WORKSPACE_DIRECTORY")]
    pub workspace_dir: PathBuf,

    /// If true, outputs will be printed instead of written to disk.
    #[clap(long)]
    pub dry_run: bool,

    /// The path to the Bazel root workspace (i.e. the directory containing the WORKSPACE.bazel file or similar).
    /// BE CAREFUL with this value. We never want to include it in a lockfile hash (to keep lockfiles portable),
    /// which means you also should not use it anywhere that _should_ be guarded by a lockfile hash.
    /// You basically never want to use this value.
    #[clap(long)]
    pub nonhermetic_root_bazel_workspace_dir: Utf8PathBuf,
}

/// Run buildifier on a given file.
fn buildifier_format(bin: &Path, file: &Path) -> anyhow::Result<ExitStatus> {
    let status = process::Command::new(bin)
        .args(["-lint=fix", "-mode=fix", "-warnings=all"])
        .arg(file)
        .status()
        .context("Failed to apply buildifier fixes")?;

    if !status.success() {
        bail!(status)
    }

    Ok(status)
}

/// Run `bazel mod tidy` in a workspace.
fn bzlmod_tidy(bin: &Path, workspace_dir: &Path) -> anyhow::Result<ExitStatus> {
    let status = process::Command::new(bin)
        .current_dir(workspace_dir)
        .arg("mod")
        .arg("tidy")
        .status()
        .context("Failed to spawn Bazel process")?;

    if !status.success() {
        bail!(status)
    }

    Ok(status)
}

/// Info about a Bazel workspace
struct BazelInfo {
    /// The version of Bazel being used
    release: semver::Version,

    /// The location of the output_user_root.
    output_base: PathBuf,
}

impl BazelInfo {
    /// Construct a new struct based on the current binary and workspace paths provided.
    fn try_new(bazel: &Path, workspace_dir: &Path) -> anyhow::Result<Self> {
        let output = process::Command::new(bazel)
            .current_dir(workspace_dir)
            .arg("info")
            .arg("release")
            .arg("output_base")
            .output()
            .context("Failed to query the Bazel workspace's `output_base`")?;

        if !output.status.success() {
            bail!(output.status)
        }

        let output = String::from_utf8_lossy(output.stdout.as_slice());
        let mut bazel_info: HashMap<String, String> = output
            .trim()
            .split('\n')
            .map(|line| {
                let (k, v) = line.split_at(
                    line.find(':')
                        .ok_or_else(|| anyhow!("missing `:` in bazel info output: `{}`", line))?,
                );
                Ok((k.to_string(), (v[1..]).trim().to_string()))
            })
            .collect::<anyhow::Result<HashMap<_, _>>>()?;

        // Allow a predefined environment variable to take precedent. This
        // solves for the specific needs of Bazel CI on Github.
        if let Ok(path) = env::var("OUTPUT_BASE") {
            bazel_info.insert("output_base".to_owned(), format!("output_base: {}", path));
        };

        BazelInfo::try_from(bazel_info)
    }
}

impl TryFrom<HashMap<String, String>> for BazelInfo {
    type Error = anyhow::Error;

    fn try_from(value: HashMap<String, String>) -> Result<Self, Self::Error> {
        Ok(BazelInfo {
            release: value
                .get("release")
                .map(|s| {
                    let mut r = s
                        .split_whitespace()
                        .last()
                        .ok_or_else(|| anyhow!("Unexpected release value: {}", s))?
                        .to_owned();

                    // Force release candidates to conform to semver.
                    if r.contains("rc") {
                        let (v, c) = r.split_once("rc").unwrap();
                        r = format!("{}-rc{}", v, c);
                    }

                    semver::Version::parse(&r).context("Failed to parse release version")
                })
                .ok_or(anyhow!("Failed to query Bazel release"))??,
            output_base: value
                .get("output_base")
                .map(Into::into)
                .ok_or(anyhow!("Failed to query Bazel output_base"))?,
        })
    }
}

pub fn vendor(opt: VendorOptions) -> anyhow::Result<()> {
    let bazel_info = BazelInfo::try_new(&opt.bazel, &opt.workspace_dir)?;

    // Load the all config files required for splicing a workspace
    let splicing_manifest = SplicingManifest::try_from_path(&opt.splicing_manifest)?
        .resolve(&opt.workspace_dir, &bazel_info.output_base);

    let temp_dir = tempfile::tempdir().context("Failed to create temporary directory")?;
    let temp_dir_path = Utf8PathBuf::from_path_buf(temp_dir.as_ref().to_path_buf())
        .unwrap_or_else(|path| panic!("Temporary directory wasn't valid UTF-8: {:?}", path));

    // Generate a splicer for creating a Cargo workspace manifest
    let splicer = Splicer::new(temp_dir_path, splicing_manifest.clone())
        .context("Failed to create splicer")?;

    let cargo = Cargo::new(opt.cargo, opt.rustc.clone());

    // Splice together the manifest
    let manifest_path = splicer
        .splice_workspace(&opt.nonhermetic_root_bazel_workspace_dir)
        .context("Failed to splice workspace")?;

    // Gather a cargo lockfile
    let cargo_lockfile = generate_lockfile(
        &manifest_path,
        &opt.cargo_lockfile,
        cargo.clone(),
        &opt.repin,
    )?;

    // Load the config from disk
    let config = Config::try_from_path(&opt.config)?;

    let resolver_data = TreeResolver::new(cargo.clone()).generate(
        manifest_path.as_path_buf(),
        &config.supported_platform_triples,
    )?;

    // Write the registry url info to the manifest now that a lockfile has been generated
    WorkspaceMetadata::write_registry_urls_and_feature_map(
        &cargo,
        &cargo_lockfile,
        resolver_data,
        manifest_path.as_path_buf(),
        manifest_path.as_path_buf(),
    )?;

    // Write metadata to the workspace for future reuse
    let cargo_metadata = cargo
        .metadata_command_with_options(
            manifest_path.as_path_buf().as_ref(),
            vec!["--locked".to_owned()],
        )?
        .exec()?;

    // Annotate metadata
    let annotations = Annotations::new(
        cargo_metadata,
        &opt.cargo_lockfile,
        cargo_lockfile.clone(),
        config.clone(),
        &opt.nonhermetic_root_bazel_workspace_dir,
    )?;

    // Generate renderable contexts for search package
    let context = Context::new(annotations, config.rendering.are_sources_present())?;

    // Render build files
    let outputs = Renderer::new(
        Arc::new(config.rendering.clone()),
        Arc::new(config.supported_platform_triples.clone()),
    )
    .render(&context, None)?;

    // First ensure vendoring and rendering happen in a clean directory
    let vendor_dir_label = render_module_label(&config.rendering.crates_module_template, "BUILD")?;
    let vendor_dir = opt.workspace_dir.join(vendor_dir_label.package().unwrap());
    if vendor_dir.exists() {
        fs::remove_dir_all(&vendor_dir)
            .with_context(|| format!("Failed to delete {}", vendor_dir.display()))?;
    }

    // Store the updated Cargo.lock
    if let Some(path) = &opt.cargo_lockfile {
        fs::write(path, cargo_lockfile.to_string())
            .context("Failed to write Cargo.lock file back to the workspace.")?;
    }

    if matches!(config.rendering.vendor_mode, Some(VendorMode::Local)) {
        VendorGenerator::new(cargo.clone(), opt.rustc.clone())
            .generate(manifest_path.as_path_buf(), &vendor_dir)
            .context("Failed to vendor dependencies")?;
    }

    // make cargo versioned crates compatible with bazel labels
    let normalized_outputs = normalize_cargo_file_paths(outputs, &opt.workspace_dir);

    // buildifier files to check
    let file_names: BTreeSet<PathBuf> = normalized_outputs.keys().cloned().collect();

    // Write outputs
    write_outputs(normalized_outputs, opt.dry_run).context("Failed writing output files")?;

    // Optionally apply buildifier fixes
    if let Some(buildifier_bin) = opt.buildifier {
        for file in file_names {
            let file_path = opt.workspace_dir.join(file);
            buildifier_format(&buildifier_bin, &file_path)
                .with_context(|| format!("Failed to run buildifier on {}", file_path.display()))?;
        }
    }

    // Optionally perform bazel mod tidy to update the MODULE.bazel file
    if bazel_info.release >= semver::Version::new(7, 0, 0) {
        let module_bazel = opt.workspace_dir.join("MODULE.bazel");
        if module_bazel.exists() {
            bzlmod_tidy(&opt.bazel, &opt.workspace_dir)?;
        }
    }

    // Write the rendering lockfile if requested.
    if let Some(lockfile) = opt.lockfile {
        let lock_content = lock_context(context, &config, &splicing_manifest, &cargo, &opt.rustc)?;

        write_lockfile(lock_content, &lockfile, opt.dry_run)?;
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bazel_info() {
        let raw_info = HashMap::from([
            ("release".to_owned(), "8.0.0".to_owned()),
            ("output_base".to_owned(), "/tmp/output_base".to_owned()),
        ]);

        let info = BazelInfo::try_from(raw_info).unwrap();

        assert_eq!(semver::Version::new(8, 0, 0), info.release);
        assert_eq!(PathBuf::from("/tmp/output_base"), info.output_base);
    }

    #[test]
    fn test_bazel_info_release_candidate() {
        let raw_info = HashMap::from([
            ("release".to_owned(), "8.0.0rc1".to_owned()),
            ("output_base".to_owned(), "/tmp/output_base".to_owned()),
        ]);

        let info = BazelInfo::try_from(raw_info).unwrap();

        assert_eq!(semver::Version::parse("8.0.0-rc1").unwrap(), info.release);
        assert_eq!(PathBuf::from("/tmp/output_base"), info.output_base);
    }

    #[test]
    fn test_bazel_info_pre_release() {
        let raw_info = HashMap::from([
            ("release".to_owned(), "9.0.0-pre.20241208.2".to_owned()),
            ("output_base".to_owned(), "/tmp/output_base".to_owned()),
        ]);

        let info = BazelInfo::try_from(raw_info).unwrap();

        assert_eq!(
            semver::Version::parse("9.0.0-pre.20241208.2").unwrap(),
            info.release
        );
        assert_eq!(PathBuf::from("/tmp/output_base"), info.output_base);
    }
}
