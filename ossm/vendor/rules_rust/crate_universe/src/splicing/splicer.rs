//! Utility for creating valid Cargo workspaces

use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

use anyhow::{bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use cargo_toml::Manifest;

use crate::config::CrateId;
use crate::metadata::discover_workspaces;
use crate::splicing::{SplicedManifest, SplicingManifest};
use crate::utils::starlark::Label;
use crate::utils::symlink::{remove_symlink, symlink};

use super::{read_manifest, DirectPackageManifest, WorkspaceMetadata};

/// The core splicer implementation. Each style of Bazel workspace should be represented
/// here and a splicing implementation defined.
pub(crate) enum SplicerKind<'a> {
    /// Splice a manifest which is represented by a Cargo workspace
    Workspace {
        path: &'a Utf8PathBuf,
        manifest: &'a Manifest,
        splicing_manifest: &'a SplicingManifest,
    },
    /// Splice a manifest for a single package. This includes cases where
    /// were defined directly in Bazel.
    Package {
        path: &'a Utf8PathBuf,
        manifest: &'a Manifest,
        splicing_manifest: &'a SplicingManifest,
    },
    /// Splice a manifest from multiple disjoint Cargo manifests.
    MultiPackage {
        manifests: &'a BTreeMap<Utf8PathBuf, Manifest>,
        splicing_manifest: &'a SplicingManifest,
    },
}

/// A list of files or directories to ignore when when symlinking
const IGNORE_LIST: &[&str] = &[".git", "bazel-*", ".svn"];

impl<'a> SplicerKind<'a> {
    pub(crate) fn new(
        manifests: &'a BTreeMap<Utf8PathBuf, Manifest>,
        splicing_manifest: &'a SplicingManifest,
        nonhermetic_root_bazel_workspace_dir: &Path,
    ) -> Result<Self> {
        let workspaces = discover_workspaces(
            manifests.keys().cloned().collect(),
            manifests,
            nonhermetic_root_bazel_workspace_dir,
        )?;
        let workspace_roots = workspaces.workspaces();
        if workspace_roots.len() > 1 {
            bail!("When splicing manifests, manifests are not allowed to from from different workspaces. Saw manifests which belong to the following workspaces: {}", workspace_roots.iter().map(|wr| wr.to_string()).collect::<Vec<_>>().join(", "));
        }

        let all_workspace_and_member_paths = workspaces.all_workspaces_and_members();
        let mut missing_labels = Vec::new();
        let mut missing_paths = Vec::new();
        for manifest_path in &all_workspace_and_member_paths {
            if !manifests.contains_key(manifest_path) {
                if let Ok(label) = Label::from_absolute_path(manifest_path.as_path().as_std_path())
                {
                    missing_labels.push(label.to_string());
                } else {
                    missing_paths.push(manifest_path.to_string());
                }
            }
        }
        if !missing_labels.is_empty() || !missing_paths.is_empty() {
            bail!(
                "Some manifests are not being tracked.{}{}",
                if !missing_labels.is_empty() {
                    format!(
                        "\nPlease add the following labels to the `manifests` key:\n {}.",
                        missing_labels.join("\n ")
                    )
                } else {
                    String::new()
                },
                if !missing_paths.is_empty() {
                    format!(
                        " Please add labels for the following paths to the `manifests` key:\n {}.",
                        missing_paths.join("\n ")
                    )
                } else {
                    String::new()
                },
            )
        }

        if let Some((path, manifest)) = workspace_roots
            .iter()
            .next()
            .and_then(|path| manifests.get_key_value(path))
        {
            Ok(Self::Workspace {
                path,
                manifest,
                splicing_manifest,
            })
        } else if manifests.len() == 1 {
            let (path, manifest) = manifests.iter().last().unwrap();
            Ok(Self::Package {
                path,
                manifest,
                splicing_manifest,
            })
        } else {
            Ok(Self::MultiPackage {
                manifests,
                splicing_manifest,
            })
        }
    }

    /// Performs splicing based on the current variant.
    #[tracing::instrument(skip_all)]
    pub(crate) fn splice(&self, workspace_dir: &Utf8Path) -> Result<SplicedManifest> {
        match self {
            SplicerKind::Workspace {
                path,
                manifest,
                splicing_manifest,
            } => Self::splice_workspace(workspace_dir, path, manifest, splicing_manifest),
            SplicerKind::Package {
                path,
                manifest,
                splicing_manifest,
            } => Self::splice_package(workspace_dir, path, manifest, splicing_manifest),
            SplicerKind::MultiPackage {
                manifests,
                splicing_manifest,
            } => Self::splice_multi_package(workspace_dir, manifests, splicing_manifest),
        }
    }

    /// Implementation for splicing Cargo workspaces
    #[tracing::instrument(skip_all)]
    fn splice_workspace(
        workspace_dir: &Utf8Path,
        path: &&Utf8PathBuf,
        manifest: &&Manifest,
        splicing_manifest: &&SplicingManifest,
    ) -> Result<SplicedManifest> {
        let mut manifest = (*manifest).clone();
        let manifest_dir = path
            .parent()
            .expect("Every manifest should havee a parent directory");

        // Link the sources of the root manifest into the new workspace
        symlink_roots(
            manifest_dir.as_std_path(),
            workspace_dir.as_std_path(),
            Some(IGNORE_LIST),
        )?;

        // Optionally install the cargo config after contents have been symlinked
        Self::setup_cargo_config(&splicing_manifest.cargo_config, workspace_dir.as_std_path())?;

        // Add any additional depeendencies to the root package
        if !splicing_manifest.direct_packages.is_empty() {
            Self::inject_direct_packages(&mut manifest, &splicing_manifest.direct_packages)?;
        }

        let root_manifest_path = workspace_dir.join("Cargo.toml");
        let member_manifests = BTreeMap::from([(*path, String::new())]);

        // Write the generated metadata to the manifest
        let workspace_metadata = WorkspaceMetadata::new(splicing_manifest, member_manifests)?;
        workspace_metadata.inject_into(&mut manifest)?;

        // Write the root manifest
        write_root_manifest(root_manifest_path.as_std_path(), manifest)?;

        Ok(SplicedManifest::Workspace(root_manifest_path))
    }

    /// Implementation for splicing individual Cargo packages
    #[tracing::instrument(skip_all)]
    fn splice_package(
        workspace_dir: &Utf8Path,
        path: &&Utf8PathBuf,
        manifest: &&Manifest,
        splicing_manifest: &&SplicingManifest,
    ) -> Result<SplicedManifest> {
        let manifest_dir = path
            .parent()
            .expect("Every manifest should havee a parent directory");

        // Link the sources of the root manifest into the new workspace
        symlink_roots(
            manifest_dir.as_std_path(),
            workspace_dir.as_std_path(),
            Some(IGNORE_LIST),
        )?;

        // Optionally install the cargo config after contents have been symlinked
        Self::setup_cargo_config(&splicing_manifest.cargo_config, workspace_dir.as_std_path())?;

        // Ensure the root package manifest has a populated `workspace` member
        let mut manifest = (*manifest).clone();
        if manifest.workspace.is_none() {
            manifest.workspace =
                default_cargo_workspace_manifest(&splicing_manifest.resolver_version).workspace
        }

        // Add any additional dependencies to the root package
        if !splicing_manifest.direct_packages.is_empty() {
            Self::inject_direct_packages(&mut manifest, &splicing_manifest.direct_packages)?;
        }

        let root_manifest_path = workspace_dir.join("Cargo.toml");
        let member_manifests = BTreeMap::from([(*path, String::new())]);

        // Write the generated metadata to the manifest
        let workspace_metadata = WorkspaceMetadata::new(splicing_manifest, member_manifests)?;
        workspace_metadata.inject_into(&mut manifest)?;

        // Write the root manifest
        write_root_manifest(root_manifest_path.as_std_path(), manifest)?;

        Ok(SplicedManifest::Package(root_manifest_path))
    }

    /// Implementation for splicing together multiple Cargo packages/workspaces
    #[tracing::instrument(skip_all)]
    fn splice_multi_package(
        workspace_dir: &Utf8Path,
        manifests: &&BTreeMap<Utf8PathBuf, Manifest>,
        splicing_manifest: &&SplicingManifest,
    ) -> Result<SplicedManifest> {
        let mut manifest = default_cargo_workspace_manifest(&splicing_manifest.resolver_version);

        // Optionally install a cargo config file into the workspace root.
        Self::setup_cargo_config(&splicing_manifest.cargo_config, workspace_dir.as_std_path())?;

        let installations =
            Self::inject_workspace_members(&mut manifest, manifests, workspace_dir.as_std_path())?;

        // Collect all patches from the manifests provided
        for (_, sub_manifest) in manifests.iter() {
            Self::inject_patches(&mut manifest, &sub_manifest.patch).with_context(|| {
                format!(
                    "Duplicate `[patch]` entries detected in {:#?}",
                    manifests
                        .keys()
                        .map(|p| p.to_string())
                        .collect::<Vec<String>>()
                )
            })?;
        }

        // Write the generated metadata to the manifest
        let workspace_metadata = WorkspaceMetadata::new(splicing_manifest, installations)?;
        workspace_metadata.inject_into(&mut manifest)?;

        // Add any additional depeendencies to the root package
        if !splicing_manifest.direct_packages.is_empty() {
            Self::inject_direct_packages(&mut manifest, &splicing_manifest.direct_packages)?;
        }

        // Write the root manifest
        let root_manifest_path = workspace_dir.join("Cargo.toml");
        write_root_manifest(root_manifest_path.as_std_path(), manifest)?;

        Ok(SplicedManifest::MultiPackage(root_manifest_path))
    }

    /// A helper for installing Cargo config files into the spliced workspace while also
    /// ensuring no other linked config file is available
    fn setup_cargo_config(
        cargo_config_path: &Option<Utf8PathBuf>,
        workspace_dir: &Path,
    ) -> Result<()> {
        // If the `.cargo` dir is a symlink, we'll need to relink it and ensure
        // a Cargo config file is omitted
        let dot_cargo_dir = workspace_dir.join(".cargo");
        if dot_cargo_dir.exists() {
            let is_symlink = dot_cargo_dir
                .symlink_metadata()
                .map(|m| m.file_type().is_symlink())
                .unwrap_or(false);
            if is_symlink {
                let real_path = dot_cargo_dir.canonicalize()?;
                remove_symlink(&dot_cargo_dir).with_context(|| {
                    format!(
                        "Failed to remove existing symlink {}",
                        dot_cargo_dir.display()
                    )
                })?;
                fs::create_dir(&dot_cargo_dir)?;
                symlink_roots(&real_path, &dot_cargo_dir, Some(&["config", "config.toml"]))?;
            } else {
                for config in [
                    dot_cargo_dir.join("config"),
                    dot_cargo_dir.join("config.toml"),
                ] {
                    if config.exists() {
                        remove_symlink(&config).with_context(|| {
                            format!(
                                "Failed to delete existing cargo config: {}",
                                config.display()
                            )
                        })?;
                    }
                }
            }
        }

        // Make sure no other config files exist
        for config in [
            workspace_dir.join("config"),
            workspace_dir.join("config.toml"),
            dot_cargo_dir.join("config"),
            dot_cargo_dir.join("config.toml"),
        ] {
            if config.exists() {
                remove_symlink(&config).with_context(|| {
                    format!(
                        "Failed to delete existing cargo config: {}",
                        config.display()
                    )
                })?;
            }
        }

        // Ensure no parent directory also has a cargo config
        let mut current_parent = workspace_dir.parent();
        while let Some(parent) = current_parent {
            let dot_cargo_dir = parent.join(".cargo");
            for config in [
                dot_cargo_dir.join("config.toml"),
                dot_cargo_dir.join("config"),
            ] {
                if config.exists() {
                    bail!(
                        "A Cargo config file was found in a parent directory to the current workspace. This is not allowed because these settings will leak into your Bazel build but will not be reproducible on other machines.\nWorkspace = {}\nCargo config = {}",
                        workspace_dir.display(),
                        config.display(),
                    )
                }
            }
            current_parent = parent.parent()
        }

        // Install the new config file after having removed all others
        if let Some(cargo_config_path) = cargo_config_path {
            if !dot_cargo_dir.exists() {
                fs::create_dir_all(&dot_cargo_dir)?;
            }

            fs::copy(cargo_config_path, dot_cargo_dir.join("config.toml"))?;
        }

        Ok(())
    }

    /// Update the newly generated manifest to include additional packages as
    /// Cargo workspace members.
    fn inject_workspace_members<'b>(
        root_manifest: &mut Manifest,
        manifests: &'b BTreeMap<Utf8PathBuf, Manifest>,
        workspace_dir: &Path,
    ) -> Result<BTreeMap<&'b Utf8PathBuf, String>> {
        manifests
            .iter()
            .map(|(path, manifest)| {
                let package_name = &manifest
                    .package
                    .as_ref()
                    .expect("Each manifest should have a root package")
                    .name;

                root_manifest
                    .workspace
                    .as_mut()
                    .expect("The root manifest is expected to always have a workspace")
                    .members
                    .push(package_name.clone());

                let manifest_dir = path
                    .parent()
                    .expect("Every manifest should havee a parent directory");

                let dest_package_dir = workspace_dir.join(package_name);

                match symlink_roots(
                    manifest_dir.as_std_path(),
                    &dest_package_dir,
                    Some(IGNORE_LIST),
                ) {
                    Ok(_) => Ok((path, package_name.clone())),
                    Err(e) => Err(e),
                }
            })
            .collect()
    }

    fn inject_direct_packages(
        manifest: &mut Manifest,
        direct_packages_manifest: &DirectPackageManifest,
    ) -> Result<()> {
        // Ensure there's a root package to satisfy Cargo requirements
        if manifest.package.is_none() {
            let new_manifest = default_cargo_package_manifest();
            manifest.package = new_manifest.package;
            if manifest.lib.is_none() {
                manifest.lib = new_manifest.lib;
            }
        }

        // Check for any duplicates
        let duplicates: Vec<&String> = manifest
            .dependencies
            .keys()
            .filter(|k| direct_packages_manifest.contains_key(*k))
            .collect();
        if !duplicates.is_empty() {
            bail!(
                "Duplications detected between manifest dependencies and direct dependencies: {:?}",
                duplicates
            )
        }

        // Add the dependencies
        for (name, details) in direct_packages_manifest.iter() {
            manifest.dependencies.insert(
                name.clone(),
                cargo_toml::Dependency::Detailed(Box::new(details.clone())),
            );
        }

        Ok(())
    }

    fn inject_patches(manifest: &mut Manifest, patches: &cargo_toml::PatchSet) -> Result<()> {
        for (registry, new_patches) in patches.iter() {
            // If there is an existing patch entry it will need to be merged
            if let Some(existing_patches) = manifest.patch.get_mut(registry) {
                // Error out if there are duplicate patches
                existing_patches.extend(
                    new_patches
                        .iter()
                        .map(|(pkg, info)| {
                            if let Some(existing_info) = existing_patches.get(pkg) {
                                // Only error if the patches are not identical
                                if existing_info != info {
                                    bail!(
                                        "Duplicate patches were found for `[patch.{}] {}`",
                                        registry,
                                        pkg
                                    );
                                }
                            }
                            Ok((pkg.clone(), info.clone()))
                        })
                        .collect::<Result<cargo_toml::DepsSet>>()?,
                );
            } else {
                manifest.patch.insert(registry.clone(), new_patches.clone());
            }
        }

        Ok(())
    }
}

pub(crate) struct Splicer {
    workspace_dir: Utf8PathBuf,
    manifests: BTreeMap<Utf8PathBuf, Manifest>,
    splicing_manifest: SplicingManifest,
}

impl Splicer {
    pub(crate) fn new(
        workspace_dir: Utf8PathBuf,
        splicing_manifest: SplicingManifest,
    ) -> Result<Self> {
        // Load all manifests
        let manifests = splicing_manifest
            .manifests
            .keys()
            .map(|path| {
                let m = read_manifest(path)
                    .with_context(|| format!("Failed to read manifest at {}", path))?;
                Ok((path.clone(), m))
            })
            .collect::<Result<BTreeMap<Utf8PathBuf, Manifest>>>()?;

        Ok(Self {
            workspace_dir,
            manifests,
            splicing_manifest,
        })
    }

    /// Build a new workspace root
    pub(crate) fn splice_workspace(
        &self,
        nonhermetic_root_bazel_workspace_dir: &Path,
    ) -> Result<SplicedManifest> {
        SplicerKind::new(
            &self.manifests,
            &self.splicing_manifest,
            nonhermetic_root_bazel_workspace_dir,
        )?
        .splice(&self.workspace_dir)
    }
}
const DEFAULT_SPLICING_PACKAGE_NAME: &str = "direct-cargo-bazel-deps";
const DEFAULT_SPLICING_PACKAGE_VERSION: &str = "0.0.1";

pub(crate) fn default_cargo_package_manifest() -> cargo_toml::Manifest {
    // A manifest is generated with a fake workspace member so the [cargo_toml::Manifest::Workspace]
    // member is deseralized and is not `None`.
    cargo_toml::Manifest::from_str(
        &toml::toml! {
            [package]
            name = DEFAULT_SPLICING_PACKAGE_NAME
            version = DEFAULT_SPLICING_PACKAGE_VERSION
            edition = "2018"

            // A fake target used to satisfy requirements of Cargo.
            [lib]
            name = "direct_cargo_bazel_deps"
            path = ".direct_cargo_bazel_deps.rs"
        }
        .to_string(),
    )
    .unwrap()
}

pub(crate) fn default_splicing_package_crate_id() -> CrateId {
    CrateId::new(
        DEFAULT_SPLICING_PACKAGE_NAME.to_string(),
        semver::Version::parse(DEFAULT_SPLICING_PACKAGE_VERSION)
            .expect("Known good version didn't parse"),
    )
}

pub(crate) fn default_cargo_workspace_manifest(
    resolver_version: &cargo_toml::Resolver,
) -> cargo_toml::Manifest {
    // A manifest is generated with a fake workspace member so the [cargo_toml::Manifest::Workspace]
    // member is deseralized and is not `None`.
    let mut manifest = cargo_toml::Manifest::from_str(&textwrap::dedent(&format!(
        r#"
            [workspace]
            resolver = "{resolver_version}"
        "#,
    )))
    .unwrap();

    // Drop the temp workspace member
    manifest.workspace.as_mut().unwrap().members.pop();

    manifest
}

pub(crate) fn write_root_manifest(path: &Path, manifest: cargo_toml::Manifest) -> Result<()> {
    // Remove the file in case one exists already, preventing symlinked files
    // from having their contents overwritten.
    if path.exists() {
        fs::remove_file(path)?;
    }

    // Ensure the directory exists
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    // Write an intermediate manifest so we can run `cargo metadata` to list all the transitive proc-macros.
    write_manifest(path, &manifest)?;

    Ok(())
}

pub(crate) fn write_manifest(path: &Path, manifest: &cargo_toml::Manifest) -> Result<()> {
    // TODO(https://gitlab.com/crates.rs/cargo_toml/-/issues/3)
    let value = toml::Value::try_from(manifest)?;
    let content = toml::to_string(&value)?;
    tracing::debug!(
        "Writing Cargo manifest '{}':\n```toml\n{}```",
        path.display(),
        content
    );
    fs::write(path, content).context(format!("Failed to write manifest to {}", path.display()))
}

/// Symlinks the root contents of a source directory into a destination directory
pub(crate) fn symlink_roots(
    source: &Path,
    dest: &Path,
    ignore_list: Option<&[&str]>,
) -> Result<()> {
    // Ensure the source exists and is a directory
    if !source.is_dir() {
        bail!("Source path is not a directory: {}", source.display());
    }

    // Only check if the dest is a directory if it already exists
    if dest.exists() && !dest.is_dir() {
        bail!("Dest path is not a directory: {}", dest.display());
    }

    fs::create_dir_all(dest)?;

    // Link each directory entry from the source dir to the dest
    for entry in (source.read_dir()?).flatten() {
        let basename = entry.file_name();

        // Ignore certain directories that may lead to confusion
        if let Some(base_str) = basename.to_str() {
            if let Some(list) = ignore_list {
                for item in list.iter() {
                    // Handle optional glob patterns here. This allows us to ignore `bazel-*` patterns.
                    if item.ends_with('*') && base_str.starts_with(item.trim_end_matches('*')) {
                        continue;
                    }

                    // Finally, simply compare the string
                    if *item == base_str {
                        continue;
                    }
                }
            }
        }

        let link_src = source.join(&basename);
        let link_dest = dest.join(&basename);
        symlink(&link_src, &link_dest).context(format!(
            "Failed to create symlink: {} -> {}",
            link_src.display(),
            link_dest.display()
        ))?;
    }

    Ok(())
}

#[cfg(test)]
mod test {
    use super::*;

    use std::fs::File;
    use std::str::FromStr;

    use cargo_metadata::PackageId;

    use crate::splicing::Cargo;

    /// Clone and compare two items after calling `.sort()` on them.
    macro_rules! assert_sort_eq {
        ($left:expr, $right:expr $(,)?) => {
            let mut left = $left.clone();
            left.sort();
            let mut right = $right.clone();
            right.sort();
            assert_eq!(left, right);
        };
    }

    fn should_skip_network_test() -> bool {
        // Some test cases require network access to build pull crate metadata
        // so that we can actually run `cargo tree`. However, RBE (and perhaps
        // other environments) disallow or don't support this. In those cases,
        // we just skip this test case.
        use std::net::ToSocketAddrs;
        if "github.com:443".to_socket_addrs().is_err() {
            eprintln!("This test case requires network access.");
            true
        } else {
            false
        }
    }

    /// Get cargo and rustc binaries the Bazel way
    #[cfg(not(feature = "cargo"))]
    fn get_cargo_and_rustc_paths() -> (std::path::PathBuf, std::path::PathBuf) {
        let r = runfiles::Runfiles::create().unwrap();
        let cargo_path = runfiles::rlocation!(r, concat!("rules_rust/", env!("CARGO"))).unwrap();
        let rustc_path = runfiles::rlocation!(r, concat!("rules_rust/", env!("RUSTC"))).unwrap();

        (cargo_path, rustc_path)
    }

    /// Get cargo and rustc binaries the Cargo way
    #[cfg(feature = "cargo")]
    fn get_cargo_and_rustc_paths() -> (PathBuf, PathBuf) {
        (PathBuf::from("cargo"), PathBuf::from("rustc"))
    }

    fn cargo() -> Cargo {
        let (cargo, rustc) = get_cargo_and_rustc_paths();
        Cargo::new(cargo, rustc)
    }

    fn generate_metadata<P: AsRef<Path>>(manifest_path: P) -> cargo_metadata::Metadata {
        cargo()
            .metadata_command_with_options(manifest_path.as_ref(), vec!["--offline".to_owned()])
            .unwrap()
            .exec()
            .unwrap()
    }

    fn mock_cargo_toml<P: AsRef<Path>>(path: P, name: &str) -> cargo_toml::Manifest {
        mock_cargo_toml_with_dependencies(path, name, &[])
    }

    fn mock_cargo_toml_with_dependencies<P: AsRef<Path>>(
        path: P,
        name: &str,
        deps: &[&str],
    ) -> cargo_toml::Manifest {
        let manifest = cargo_toml::Manifest::from_str(&textwrap::dedent(&format!(
            r#"
            [package]
            name = "{name}"
            version = "0.0.1"

            [lib]
            path = "lib.rs"

            [dependencies]
            {dependencies}
            "#,
            name = name,
            dependencies = deps.join("\n")
        )))
        .unwrap();

        let path = path.as_ref();
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(path, toml::to_string(&manifest).unwrap()).unwrap();

        manifest
    }

    fn mock_workspace_metadata(
        include_extra_member: bool,
        workspace_prefix: Option<&str>,
    ) -> serde_json::Value {
        let mut obj = if include_extra_member {
            serde_json::json!({
                "cargo-bazel": {
                    "package_prefixes": {},
                    "sources": {
                        "extra_pkg 0.0.1": {
                            "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                            "url": "https://crates.io/"
                        }
                    },
                    "tree_metadata": {}
                }
            })
        } else {
            serde_json::json!({
                "cargo-bazel": {
                    "package_prefixes": {},
                    "sources": {},
                    "tree_metadata": {}
                }
            })
        };
        if let Some(workspace_prefix) = workspace_prefix {
            obj.as_object_mut().unwrap()["cargo-bazel"]
                .as_object_mut()
                .unwrap()
                .insert("workspace_prefix".to_owned(), workspace_prefix.into());
        }
        obj
    }

    fn mock_splicing_manifest_with_workspace() -> (SplicingManifest, tempfile::TempDir) {
        let mut splicing_manifest = SplicingManifest::default();
        let cache_dir = tempfile::tempdir().unwrap();

        // Write workspace members
        for pkg in &["sub_pkg_a", "sub_pkg_b"] {
            let manifest_path = Utf8PathBuf::try_from(
                cache_dir
                    .as_ref()
                    .join("root_pkg")
                    .join(pkg)
                    .join("Cargo.toml"),
            )
            .unwrap();
            let deps = if pkg == &"sub_pkg_b" {
                vec![r#"sub_pkg_a = { path = "../sub_pkg_a" }"#]
            } else {
                vec![]
            };
            mock_cargo_toml_with_dependencies(&manifest_path, pkg, &deps);

            splicing_manifest.manifests.insert(
                manifest_path,
                Label::from_str(&format!("//{pkg}:Cargo.toml")).unwrap(),
            );
        }

        // Create the root package with a workspace definition
        let manifest: cargo_toml::Manifest = toml::toml! {
            [workspace]
            members = [
                "sub_pkg_a",
                "sub_pkg_b",
            ]
            [package]
            name = "root_pkg"
            version = "0.0.1"

            [lib]
            path = "lib.rs"
        }
        .try_into()
        .unwrap();

        let workspace_root = cache_dir.as_ref();
        {
            File::create(workspace_root.join("WORKSPACE.bazel")).unwrap();
        }
        let root_pkg = workspace_root.join("root_pkg");
        let manifest_path = Utf8PathBuf::try_from(root_pkg.join("Cargo.toml")).unwrap();
        fs::create_dir_all(manifest_path.parent().unwrap()).unwrap();
        fs::write(&manifest_path, toml::to_string(&manifest).unwrap()).unwrap();
        {
            File::create(root_pkg.join("BUILD.bazel")).unwrap();
        }

        splicing_manifest.manifests.insert(
            manifest_path,
            Label::from_str("//root_pkg:Cargo.toml").unwrap(),
        );

        for sub_pkg in ["sub_pkg_a", "sub_pkg_b"] {
            let sub_pkg_path = root_pkg.join(sub_pkg);
            fs::create_dir_all(&sub_pkg_path).unwrap();
            File::create(sub_pkg_path.join("BUILD.bazel")).unwrap();
        }

        (splicing_manifest, cache_dir)
    }

    fn mock_splicing_manifest_with_workspace_in_root() -> (SplicingManifest, tempfile::TempDir) {
        let mut splicing_manifest = SplicingManifest::default();
        let cache_dir = tempfile::tempdir().unwrap();

        // Write workspace members
        for pkg in &["sub_pkg_a", "sub_pkg_b"] {
            let manifest_path =
                Utf8PathBuf::try_from(cache_dir.as_ref().join(pkg).join("Cargo.toml")).unwrap();
            mock_cargo_toml(&manifest_path, pkg);

            splicing_manifest.manifests.insert(
                manifest_path,
                Label::from_str(&format!("//{pkg}:Cargo.toml")).unwrap(),
            );
        }

        // Create the root package with a workspace definition
        let manifest: cargo_toml::Manifest = toml::toml! {
            [workspace]
            members = [
                "sub_pkg_a",
                "sub_pkg_b",
            ]
            [package]
            name = "root_pkg"
            version = "0.0.1"

            [lib]
            path = "lib.rs"
        }
        .try_into()
        .unwrap();

        let workspace_root = cache_dir.as_ref();
        {
            File::create(workspace_root.join("WORKSPACE.bazel")).unwrap();
        }
        let manifest_path = Utf8PathBuf::try_from(workspace_root.join("Cargo.toml")).unwrap();
        fs::create_dir_all(manifest_path.parent().unwrap()).unwrap();
        fs::write(&manifest_path, toml::to_string(&manifest).unwrap()).unwrap();

        splicing_manifest
            .manifests
            .insert(manifest_path, Label::from_str("//:Cargo.toml").unwrap());

        for sub_pkg in ["sub_pkg_a", "sub_pkg_b"] {
            let sub_pkg_path = workspace_root.join(sub_pkg);
            fs::create_dir_all(&sub_pkg_path).unwrap();
            File::create(sub_pkg_path.join("BUILD.bazel")).unwrap();
        }

        (splicing_manifest, cache_dir)
    }

    fn mock_splicing_manifest_with_package() -> (SplicingManifest, tempfile::TempDir) {
        let mut splicing_manifest = SplicingManifest::default();
        let cache_dir = tempfile::tempdir().unwrap();

        // Add an additional package
        let manifest_path =
            Utf8PathBuf::try_from(cache_dir.as_ref().join("root_pkg").join("Cargo.toml")).unwrap();
        mock_cargo_toml(&manifest_path, "root_pkg");
        splicing_manifest
            .manifests
            .insert(manifest_path, Label::from_str("//:Cargo.toml").unwrap());

        (splicing_manifest, cache_dir)
    }

    fn mock_splicing_manifest_with_multi_package() -> (SplicingManifest, tempfile::TempDir) {
        let mut splicing_manifest = SplicingManifest::default();
        let cache_dir = tempfile::tempdir().unwrap();

        // Add an additional package
        for pkg in &["pkg_a", "pkg_b", "pkg_c"] {
            let manifest_path =
                Utf8PathBuf::try_from(cache_dir.as_ref().join(pkg).join("Cargo.toml")).unwrap();
            mock_cargo_toml(&manifest_path, pkg);
            splicing_manifest
                .manifests
                .insert(manifest_path, Label::from_str("//:Cargo.toml").unwrap());
        }

        (splicing_manifest, cache_dir)
    }

    fn new_package_id(
        name: &str,
        workspace_root: &Path,
        is_root: bool,
        cargo: &Cargo,
    ) -> PackageId {
        let mut workspace_root = workspace_root.display().to_string();

        // On windows, make sure we normalize the path to match what Cargo would
        // otherwise use to populate metadata.
        if cfg!(target_os = "windows") {
            workspace_root = format!("/{}", workspace_root.replace('\\', "/"))
        };

        // Cargo updated the way package id's are represented. We should make sure
        // to render the correct version based on the current cargo binary.
        let use_format_v2 = cargo.uses_new_package_id_format().expect(
            "Tests should have a fully controlled environment and consistent access to cargo.",
        );

        if is_root {
            PackageId {
                repr: if use_format_v2 {
                    format!("path+file://{workspace_root}#{name}@0.0.1")
                } else {
                    format!("{name} 0.0.1 (path+file://{workspace_root})")
                },
            }
        } else {
            PackageId {
                repr: if use_format_v2 {
                    format!("path+file://{workspace_root}/{name}#0.0.1")
                } else {
                    format!("{name} 0.0.1 (path+file://{workspace_root}/{name})")
                },
            }
        }
    }

    #[test]
    fn splice_workspace() {
        let (splicing_manifest, _cache_dir) = mock_splicing_manifest_with_workspace_in_root();

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Locate cargo
        let cargo = cargo();

        // Ensure metadata is valid
        let metadata = generate_metadata(workspace_manifest.as_path_buf());
        assert_sort_eq!(
            metadata.workspace_members,
            vec![
                new_package_id("sub_pkg_a", workspace_root.as_ref(), false, &cargo),
                new_package_id("sub_pkg_b", workspace_root.as_ref(), false, &cargo),
                new_package_id("root_pkg", workspace_root.as_ref(), true, &cargo),
            ]
        );

        // Ensure the workspace metadata annotations are populated
        assert_eq!(
            metadata.workspace_metadata,
            mock_workspace_metadata(false, None)
        );

        // Since no direct packages were added to the splicing manifest, the cargo_bazel
        // deps target should __not__ have been injected into the manifest.
        assert!(!metadata
            .packages
            .iter()
            .any(|pkg| pkg.name == DEFAULT_SPLICING_PACKAGE_NAME));

        // Ensure lockfile was successfully spliced
        cargo_lock::Lockfile::load(workspace_root.as_ref().join("Cargo.lock")).unwrap();
    }

    #[test]
    fn splice_workspace_in_root() {
        let (splicing_manifest, _cache_dir) = mock_splicing_manifest_with_workspace_in_root();

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Locate cargo
        let cargo = cargo();

        // Ensure metadata is valid
        let metadata = generate_metadata(workspace_manifest.as_path_buf());
        assert_sort_eq!(
            metadata.workspace_members,
            vec![
                new_package_id("sub_pkg_a", workspace_root.as_ref(), false, &cargo),
                new_package_id("sub_pkg_b", workspace_root.as_ref(), false, &cargo),
                new_package_id("root_pkg", workspace_root.as_ref(), true, &cargo),
            ]
        );

        // Ensure the workspace metadata annotations are populated
        assert_eq!(
            metadata.workspace_metadata,
            mock_workspace_metadata(false, None)
        );

        // Since no direct packages were added to the splicing manifest, the cargo_bazel
        // deps target should __not__ have been injected into the manifest.
        assert!(!metadata
            .packages
            .iter()
            .any(|pkg| pkg.name == DEFAULT_SPLICING_PACKAGE_NAME));

        // Ensure lockfile was successfully spliced
        cargo_lock::Lockfile::load(workspace_root.as_ref().join("Cargo.lock")).unwrap();
    }

    #[test]
    fn splice_workspace_report_missing_members() {
        let (mut splicing_manifest, _cache_dir) = mock_splicing_manifest_with_workspace();

        // Remove everything but the root manifest
        splicing_manifest
            .manifests
            .retain(|_, label| *label == Label::from_str("//root_pkg:Cargo.toml").unwrap());
        assert_eq!(splicing_manifest.manifests.len(), 1);

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest = Splicer::new(
            Utf8PathBuf::try_from(workspace_root.as_ref().to_path_buf()).unwrap(),
            splicing_manifest,
        )
        .unwrap()
        .splice_workspace(Path::new("/doesnotexist"));

        assert!(workspace_manifest.is_err());

        // Ensure both the missing manifests are mentioned in the error string
        let err_str = format!("{:?}", &workspace_manifest);
        assert!(
            err_str.contains("Some manifests are not being tracked")
                && err_str.contains("//root_pkg/sub_pkg_a:Cargo.toml")
                && err_str.contains("//root_pkg/sub_pkg_b:Cargo.toml")
        );
    }

    #[test]
    fn splice_workspace_report_missing_root() {
        let (mut splicing_manifest, _cache_dir) = mock_splicing_manifest_with_workspace();

        // Remove everything but the root manifest
        splicing_manifest
            .manifests
            .retain(|_, label| *label != Label::from_str("//root_pkg:Cargo.toml").unwrap());
        assert_eq!(splicing_manifest.manifests.len(), 2);

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest = Splicer::new(
            Utf8PathBuf::try_from(workspace_root.as_ref().to_path_buf()).unwrap(),
            splicing_manifest,
        )
        .unwrap()
        .splice_workspace(Path::new("/doesnotexist"));

        assert!(workspace_manifest.is_err());

        // Ensure both the missing manifests are mentioned in the error string
        let err_str = format!("{:?}", &workspace_manifest);
        assert!(
            err_str.contains("Some manifests are not being tracked")
                && err_str.contains("//root_pkg:Cargo.toml")
        );
    }

    #[test]
    fn splice_workspace_report_external_workspace_members() {
        let (mut splicing_manifest, _cache_dir) = mock_splicing_manifest_with_workspace();

        // Add a new package from an existing external workspace
        let external_workspace_root = tempfile::tempdir().unwrap();
        let external_manifest = Utf8PathBuf::try_from(
            external_workspace_root
                .as_ref()
                .join("external_workspace_member")
                .join("Cargo.toml"),
        )
        .unwrap();
        fs::create_dir_all(external_manifest.parent().unwrap()).unwrap();

        fs::write(
            external_workspace_root.as_ref().join("Cargo.toml"),
            textwrap::dedent(
                r#"
                [workspace]
                [package]
                name = "external_workspace_root"
                version = "0.0.1"

                [lib]
                path = "lib.rs"
                "#,
            ),
        )
        .unwrap();

        fs::write(
            &external_manifest,
            textwrap::dedent(
                r#"
                [package]
                name = "external_workspace_member"
                version = "0.0.1"

                [lib]
                path = "lib.rs"
                "#,
            ),
        )
        .unwrap();

        splicing_manifest.manifests.insert(
            external_manifest.clone(),
            Label::from_str("@remote_dep//external_workspace_member:Cargo.toml").unwrap(),
        );

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"));

        assert!(workspace_manifest.is_err());

        // Ensure both the external workspace member
        let err_str = format!("{:?}", &workspace_manifest);
        assert!(
            err_str
                .contains("When splicing manifests, manifests are not allowed to from from different workspaces. Saw manifests which belong to the following workspaces:")
                && err_str.contains(external_workspace_root.path().to_string_lossy().as_ref())
        );
    }

    #[test]
    fn splice_workspace_no_root_pkg() {
        let (splicing_manifest, cache_dir) = mock_splicing_manifest_with_workspace_in_root();

        // Modify the root manifest to remove the rendered package
        fs::write(
            cache_dir.as_ref().join("Cargo.toml"),
            textwrap::dedent(
                r#"
                [workspace]
                members = [
                    "sub_pkg_a",
                    "sub_pkg_b",
                ]
                "#,
            ),
        )
        .unwrap();

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        let metadata = generate_metadata(workspace_manifest.as_path_buf());

        // Since no direct packages were added to the splicing manifest, the cargo_bazel
        // deps target should __not__ have been injected into the manifest.
        assert!(!metadata
            .packages
            .iter()
            .any(|pkg| pkg.name == DEFAULT_SPLICING_PACKAGE_NAME));

        // Ensure lockfile was successfully spliced
        cargo_lock::Lockfile::load(workspace_root.as_ref().join("Cargo.lock")).unwrap();
    }

    #[test]
    fn splice_package() {
        let (splicing_manifest, _cache_dir) = mock_splicing_manifest_with_package();

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Locate cargo
        let cargo = cargo();

        // Ensure metadata is valid
        let metadata = generate_metadata(workspace_manifest.as_path_buf());
        assert_sort_eq!(
            metadata.workspace_members,
            vec![new_package_id(
                "root_pkg",
                workspace_root.as_ref(),
                true,
                &cargo
            )]
        );

        // Ensure the workspace metadata annotations are not populated
        assert_eq!(
            metadata.workspace_metadata,
            mock_workspace_metadata(false, None)
        );

        // Ensure lockfile was successfully spliced
        cargo_lock::Lockfile::load(workspace_root.as_ref().join("Cargo.lock")).unwrap();
    }

    #[test]
    fn splice_multi_package() {
        let (splicing_manifest, _cache_dir) = mock_splicing_manifest_with_multi_package();

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Check the default resolver version
        let cargo_manifest = cargo_toml::Manifest::from_str(
            &fs::read_to_string(workspace_manifest.as_path_buf()).unwrap(),
        )
        .unwrap();
        assert!(cargo_manifest.workspace.is_some());
        assert_eq!(
            cargo_manifest.workspace.unwrap().resolver,
            Some(cargo_toml::Resolver::V1)
        );

        // Locate cargo
        let cargo = cargo();

        // Ensure metadata is valid
        let metadata = generate_metadata(workspace_manifest.as_path_buf());
        assert_sort_eq!(
            metadata.workspace_members,
            vec![
                new_package_id("pkg_a", workspace_root.as_ref(), false, &cargo),
                new_package_id("pkg_b", workspace_root.as_ref(), false, &cargo),
                new_package_id("pkg_c", workspace_root.as_ref(), false, &cargo),
            ]
        );

        // Ensure the workspace metadata annotations are populated
        assert_eq!(
            metadata.workspace_metadata,
            mock_workspace_metadata(false, None)
        );

        // Ensure lockfile was successfully spliced
        cargo_lock::Lockfile::load(workspace_root.as_ref().join("Cargo.lock")).unwrap();
    }

    #[test]
    fn splice_multi_package_with_resolver() {
        let (mut splicing_manifest, _cache_dir) = mock_splicing_manifest_with_multi_package();

        // Update the resolver version
        splicing_manifest.resolver_version = cargo_toml::Resolver::V2;

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Check the specified resolver version
        let cargo_manifest = cargo_toml::Manifest::from_str(
            &fs::read_to_string(workspace_manifest.as_path_buf()).unwrap(),
        )
        .unwrap();
        assert!(cargo_manifest.workspace.is_some());
        assert_eq!(
            cargo_manifest.workspace.unwrap().resolver,
            Some(cargo_toml::Resolver::V2)
        );

        // Locate cargo
        let cargo = cargo();

        // Ensure metadata is valid
        let metadata = generate_metadata(workspace_manifest.as_path_buf());
        assert_sort_eq!(
            metadata.workspace_members,
            vec![
                new_package_id("pkg_a", workspace_root.as_ref(), false, &cargo),
                new_package_id("pkg_b", workspace_root.as_ref(), false, &cargo),
                new_package_id("pkg_c", workspace_root.as_ref(), false, &cargo),
            ]
        );

        // Ensure the workspace metadata annotations are populated
        assert_eq!(
            metadata.workspace_metadata,
            mock_workspace_metadata(false, None)
        );

        // Ensure lockfile was successfully spliced
        cargo_lock::Lockfile::load(workspace_root.as_ref().join("Cargo.lock")).unwrap();
    }

    #[test]
    fn splice_multi_package_with_direct_deps() {
        if should_skip_network_test() {
            return;
        }

        let (mut splicing_manifest, _cache_dir) = mock_splicing_manifest_with_multi_package();

        // Add a "direct dependency" entry
        splicing_manifest.direct_packages.insert(
            "syn".to_owned(),
            cargo_toml::DependencyDetail {
                version: Some("1.0.109".to_owned()),
                ..syn_dependency_detail()
            },
        );

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Check the default resolver version
        let cargo_manifest = cargo_toml::Manifest::from_str(
            &fs::read_to_string(workspace_manifest.as_path_buf()).unwrap(),
        )
        .unwrap();

        // Due to the addition of direct deps for splicing, this package should have been added to the root manfiest.
        assert!(cargo_manifest.package.unwrap().name == DEFAULT_SPLICING_PACKAGE_NAME);
    }

    #[test]
    fn splice_multi_package_with_patch() {
        if should_skip_network_test() {
            return;
        }

        let (splicing_manifest, cache_dir) = mock_splicing_manifest_with_multi_package();

        // Generate a patch entry
        let expected = cargo_toml::PatchSet::from([(
            "crates-io".to_owned(),
            BTreeMap::from([(
                "syn".to_owned(),
                cargo_toml::Dependency::Detailed(Box::new(syn_dependency_detail())),
            )]),
        )]);

        // Insert the patch entry to the manifests
        let manifest_path = cache_dir.as_ref().join("pkg_a").join("Cargo.toml");
        let mut manifest =
            cargo_toml::Manifest::from_str(&fs::read_to_string(&manifest_path).unwrap()).unwrap();
        manifest.patch.extend(expected.clone());
        fs::write(manifest_path, toml::to_string(&manifest).unwrap()).unwrap();

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Ensure the patches match the expected value
        let cargo_manifest = cargo_toml::Manifest::from_str(
            &fs::read_to_string(workspace_manifest.as_path_buf()).unwrap(),
        )
        .unwrap();
        assert_eq!(expected, cargo_manifest.patch);
    }

    #[test]
    fn splice_multi_package_with_merged_patch_registries() {
        if should_skip_network_test() {
            return;
        }

        let (splicing_manifest, cache_dir) = mock_splicing_manifest_with_multi_package();

        let expected = cargo_toml::PatchSet::from([(
            "crates-io".to_owned(),
            cargo_toml::DepsSet::from([
                (
                    "syn".to_owned(),
                    cargo_toml::Dependency::Detailed(Box::new(syn_dependency_detail())),
                ),
                (
                    "lazy_static".to_owned(),
                    cargo_toml::Dependency::Detailed(Box::new(lazy_static_dependency_detail())),
                ),
            ]),
        )]);

        for pkg in ["pkg_a", "pkg_b"] {
            // Generate a patch entry
            let mut map = BTreeMap::new();
            if pkg == "pkg_a" {
                map.insert(
                    "syn".to_owned(),
                    cargo_toml::Dependency::Detailed(Box::new(syn_dependency_detail())),
                );
            } else {
                map.insert(
                    "lazy_static".to_owned(),
                    cargo_toml::Dependency::Detailed(Box::new(lazy_static_dependency_detail())),
                );
            }
            let new_patch = cargo_toml::PatchSet::from([("crates-io".to_owned(), map)]);

            // Insert the patch entry to the manifests
            let manifest_path = cache_dir.as_ref().join(pkg).join("Cargo.toml");
            let mut manifest =
                cargo_toml::Manifest::from_str(&fs::read_to_string(&manifest_path).unwrap())
                    .unwrap();
            manifest.patch.extend(new_patch);
            fs::write(manifest_path, toml::to_string(&manifest).unwrap()).unwrap();
        }

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Ensure the patches match the expected value
        let cargo_manifest = cargo_toml::Manifest::from_str(
            &fs::read_to_string(workspace_manifest.as_path_buf()).unwrap(),
        )
        .unwrap();
        assert_eq!(expected, cargo_manifest.patch);
    }

    #[test]
    fn splice_multi_package_with_merged_identical_patch_registries() {
        if should_skip_network_test() {
            return;
        }

        let (splicing_manifest, cache_dir) = mock_splicing_manifest_with_multi_package();

        let expected = cargo_toml::PatchSet::from([(
            "crates-io".to_owned(),
            cargo_toml::DepsSet::from([(
                "syn".to_owned(),
                cargo_toml::Dependency::Detailed(Box::new(syn_dependency_detail())),
            )]),
        )]);

        for pkg in ["pkg_a", "pkg_b"] {
            // Generate a patch entry
            let new_patch = cargo_toml::PatchSet::from([(
                "crates-io".to_owned(),
                BTreeMap::from([(
                    "syn".to_owned(),
                    cargo_toml::Dependency::Detailed(Box::new(syn_dependency_detail())),
                )]),
            )]);

            // Insert the patch entry to the manifests
            let manifest_path = cache_dir.as_ref().join(pkg).join("Cargo.toml");
            let mut manifest =
                cargo_toml::Manifest::from_str(&fs::read_to_string(&manifest_path).unwrap())
                    .unwrap();
            manifest.patch.extend(new_patch);
            fs::write(manifest_path, toml::to_string(&manifest).unwrap()).unwrap();
        }

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let workspace_manifest =
            Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
                .unwrap()
                .splice_workspace(Path::new("/doesnotexist"))
                .unwrap();

        // Ensure the patches match the expected value
        let cargo_manifest = cargo_toml::Manifest::from_str(
            &fs::read_to_string(workspace_manifest.as_path_buf()).unwrap(),
        )
        .unwrap();
        assert_eq!(expected, cargo_manifest.patch);
    }

    #[test]
    fn splice_multi_package_with_conflicting_patch() {
        let (splicing_manifest, cache_dir) = mock_splicing_manifest_with_multi_package();

        let mut patch = 3;
        for pkg in ["pkg_a", "pkg_b"] {
            // Generate a patch entry
            let new_patch = cargo_toml::PatchSet::from([(
                "registry".to_owned(),
                BTreeMap::from([(
                    "foo".to_owned(),
                    cargo_toml::Dependency::Simple(format!("1.2.{patch}")),
                )]),
            )]);

            // Increment the patch semver to make the patch info unique.
            patch += 1;

            // Insert the patch entry to the manifests
            let manifest_path = cache_dir.as_ref().join(pkg).join("Cargo.toml");
            let mut manifest =
                cargo_toml::Manifest::from_str(&fs::read_to_string(&manifest_path).unwrap())
                    .unwrap();
            manifest.patch.extend(new_patch);
            fs::write(manifest_path, toml::to_string(&manifest).unwrap()).unwrap();
        }

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        let result = Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
            .unwrap()
            .splice_workspace(Path::new("/doesnotexist"));

        // Confirm conflicting patches have been detected
        assert!(result.is_err());
        let err_str = result.err().unwrap().to_string();
        assert!(err_str.starts_with("Duplicate `[patch]` entries detected in"));
    }

    #[test]
    fn cargo_config_setup() {
        let (mut splicing_manifest, _cache_dir) = mock_splicing_manifest_with_workspace_in_root();

        // Write a cargo config
        let temp_dir = tempfile::tempdir().unwrap();
        let external_config = tempdir_utf8pathbuf(&temp_dir).join("config.toml");
        fs::write(&external_config, "# Cargo configuration file").unwrap();
        splicing_manifest.cargo_config = Some(external_config);

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
            .unwrap()
            .splice_workspace(Path::new("/doesnotexist"))
            .unwrap();

        let cargo_config = workspace_root.as_ref().join(".cargo").join("config.toml");
        assert!(cargo_config.exists());
        assert_eq!(
            fs::read_to_string(cargo_config).unwrap().trim(),
            "# Cargo configuration file"
        );
    }

    #[test]
    fn unregistered_cargo_config_replaced() {
        let (mut splicing_manifest, cache_dir) = mock_splicing_manifest_with_workspace_in_root();

        // Generate a cargo config that is not tracked by the splicing manifest
        fs::create_dir_all(cache_dir.as_ref().join(".cargo")).unwrap();
        fs::write(
            cache_dir.as_ref().join(".cargo").join("config.toml"),
            "# Untracked Cargo configuration file",
        )
        .unwrap();

        // Write a cargo config
        let temp_dir = tempfile::tempdir().unwrap();
        let external_config = tempdir_utf8pathbuf(&temp_dir).join("config.toml");
        fs::write(&external_config, "# Cargo configuration file").unwrap();
        splicing_manifest.cargo_config = Some(external_config);

        // Splice the workspace
        let workspace_root = tempfile::tempdir().unwrap();
        Splicer::new(tempdir_utf8pathbuf(&workspace_root), splicing_manifest)
            .unwrap()
            .splice_workspace(Path::new("/doesnotexist"))
            .unwrap();

        let cargo_config = workspace_root.as_ref().join(".cargo").join("config.toml");
        assert!(cargo_config.exists());
        assert_eq!(
            fs::read_to_string(cargo_config).unwrap().trim(),
            "# Cargo configuration file"
        );
    }

    #[test]
    fn error_on_cargo_config_in_parent() {
        let (mut splicing_manifest, _cache_dir) = mock_splicing_manifest_with_workspace_in_root();

        // Write a cargo config
        let temp_dir = tempfile::tempdir().unwrap();
        let dot_cargo_dir = tempdir_utf8pathbuf(&temp_dir).join(".cargo");
        fs::create_dir_all(&dot_cargo_dir).unwrap();
        let external_config = dot_cargo_dir.join("config.toml");
        fs::write(&external_config, "# Cargo configuration file").unwrap();
        splicing_manifest.cargo_config = Some(external_config.clone());

        // Splice the workspace
        let workspace_root = tempdir_utf8pathbuf(&temp_dir).join("workspace_root");
        let splicing_result = Splicer::new(workspace_root.clone(), splicing_manifest)
            .unwrap()
            .splice_workspace(Path::new("/doesnotexist"));

        // Ensure cargo config files in parent directories lead to errors
        assert!(splicing_result.is_err());
        let err_str = splicing_result.err().unwrap().to_string();
        assert!(err_str.starts_with("A Cargo config file was found in a parent directory"));
        assert!(err_str.contains(&format!("Workspace = {}", workspace_root)));
        assert!(err_str.contains(&format!("Cargo config = {}", external_config)));
    }

    fn syn_dependency_detail() -> cargo_toml::DependencyDetail {
        cargo_toml::DependencyDetail {
            git: Some("https://github.com/dtolnay/syn.git".to_owned()),
            tag: Some("1.0.109".to_owned()),
            ..cargo_toml::DependencyDetail::default()
        }
    }

    fn lazy_static_dependency_detail() -> cargo_toml::DependencyDetail {
        cargo_toml::DependencyDetail {
            git: Some("https://github.com/rust-lang-nursery/lazy-static.rs.git".to_owned()),
            tag: Some("1.5.0".to_owned()),
            ..cargo_toml::DependencyDetail::default()
        }
    }

    fn tempdir_utf8pathbuf(tempdir: &tempfile::TempDir) -> Utf8PathBuf {
        Utf8PathBuf::try_from(tempdir.as_ref().to_path_buf()).unwrap()
    }
}
