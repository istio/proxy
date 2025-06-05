//! This module is responsible for finding a Cargo workspace

pub(crate) mod cargo_config;
mod crate_index_lookup;
mod splicer;

use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use anyhow::{anyhow, bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use cargo_lock::package::SourceKind;
use cargo_toml::Manifest;
use serde::{Deserialize, Serialize};

use crate::config::CrateId;
use crate::metadata::{Cargo, CargoUpdateRequest, LockGenerator, TreeResolverMetadata};
use crate::utils;
use crate::utils::starlark::Label;

use self::cargo_config::CargoConfig;
use self::crate_index_lookup::CrateIndexLookup;
pub(crate) use self::splicer::*;

type DirectPackageManifest = BTreeMap<String, cargo_toml::DependencyDetail>;

/// A collection of information used for splicing together a new Cargo manifest.
#[derive(Debug, Default, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub(crate) struct SplicingManifest {
    /// A set of all packages directly written to the rule
    pub(crate) direct_packages: DirectPackageManifest,

    /// A mapping of manifest paths to the labels representing them
    pub(crate) manifests: BTreeMap<Utf8PathBuf, Label>,

    /// The path of a Cargo config file
    pub(crate) cargo_config: Option<Utf8PathBuf>,

    /// The Cargo resolver version to use for splicing
    pub(crate) resolver_version: cargo_toml::Resolver,
}

impl FromStr for SplicingManifest {
    type Err = serde_json::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        serde_json::from_str(s)
    }
}

impl SplicingManifest {
    pub(crate) fn try_from_path<T: AsRef<Path>>(path: T) -> Result<Self> {
        let content = fs::read_to_string(path.as_ref())?;
        Self::from_str(&content).context("Failed to load SplicingManifest")
    }

    pub(crate) fn resolve(self, workspace_dir: &Path, output_base: &Path) -> Self {
        let Self {
            manifests,
            cargo_config,
            ..
        } = self;

        let workspace_dir_str = workspace_dir.to_string_lossy();
        let output_base_str = output_base.to_string_lossy();

        // Ensure manifests all have absolute paths
        let manifests = manifests
            .into_iter()
            .map(|(path, label)| {
                let resolved_path = path
                    .to_string()
                    .replace("${build_workspace_directory}", &workspace_dir_str)
                    .replace("${output_base}", &output_base_str);
                (Utf8PathBuf::from(resolved_path), label)
            })
            .collect();

        // Ensure the cargo config is located at an absolute path
        let cargo_config = cargo_config.map(|path| {
            let resolved_path = path
                .to_string()
                .replace("${build_workspace_directory}", &workspace_dir_str)
                .replace("${output_base}", &output_base_str);
            Utf8PathBuf::from(resolved_path)
        });

        Self {
            manifests,
            cargo_config,
            ..self
        }
    }
}

/// The result of fully resolving a [SplicingManifest] in preparation for splicing.
#[derive(Debug, Serialize, Default)]
pub(crate) struct SplicingMetadata {
    /// A set of all packages directly written to the rule
    pub(crate) direct_packages: DirectPackageManifest,

    /// A mapping of manifest paths to the labels representing them
    pub(crate) manifests: BTreeMap<Label, cargo_toml::Manifest>,

    /// The path of a Cargo config file
    pub(crate) cargo_config: Option<CargoConfig>,
}

impl TryFrom<SplicingManifest> for SplicingMetadata {
    type Error = anyhow::Error;

    fn try_from(value: SplicingManifest) -> Result<Self, Self::Error> {
        let direct_packages = value.direct_packages;

        let manifests = value
            .manifests
            .into_iter()
            .map(|(path, label)| {
                // We read the content of a manifest file to buffer and use `from_slice` to
                // parse it. The reason is that the `from_path` version will resolve indirect
                // path dependencies in the workspace to absolute path, which causes the hash
                // to be unstable. Not resolving implicit data is okay here because the
                // workspace manifest is also included in the hash.
                // See https://github.com/bazelbuild/rules_rust/issues/2016
                let manifest_content = fs::read(&path)
                    .with_context(|| format!("Failed to load manifest '{}'", path))?;
                let manifest = cargo_toml::Manifest::from_slice(&manifest_content)
                    .with_context(|| format!("Failed to parse manifest '{}'", path))?;
                Ok((label, manifest))
            })
            .collect::<Result<BTreeMap<Label, Manifest>>>()?;

        let cargo_config = match value.cargo_config {
            Some(path) => Some(
                CargoConfig::try_from_path(path.as_std_path())
                    .with_context(|| format!("Failed to load cargo config '{}'", path))?,
            ),
            None => None,
        };

        Ok(Self {
            direct_packages,
            manifests,
            cargo_config,
        })
    }
}

#[derive(Debug, Default, Serialize, Deserialize, Clone)]
pub(crate) struct SourceInfo {
    /// A url where to a `.crate` file.
    pub(crate) url: String,

    /// The `.crate` file's sha256 checksum.
    pub(crate) sha256: String,
}

/// Information about the Cargo workspace relative to the Bazel workspace
#[derive(Debug, Default, Serialize, Deserialize)]
pub(crate) struct WorkspaceMetadata {
    /// A mapping of crates to information about where their source can be downloaded
    pub(crate) sources: BTreeMap<CrateId, SourceInfo>,

    /// The path from the root of a Bazel workspace to the root of the Cargo workspace
    pub(crate) workspace_prefix: Option<String>,

    /// Paths from the root of a Bazel workspace to a Cargo package
    pub(crate) package_prefixes: BTreeMap<String, String>,

    /// Feature set for each target triplet and crate.
    ///
    /// We store this here because it's computed during the splicing phase via
    /// calls to "cargo tree" which need the full spliced workspace.
    pub(crate) tree_metadata: TreeResolverMetadata,
}

impl TryFrom<toml::Value> for WorkspaceMetadata {
    type Error = anyhow::Error;

    fn try_from(value: toml::Value) -> Result<Self, Self::Error> {
        match value.get("cargo-bazel") {
            Some(v) => v
                .to_owned()
                .try_into()
                .context("Failed to deserialize toml value"),
            None => bail!("cargo-bazel workspace metadata not found"),
        }
    }
}

impl TryFrom<serde_json::Value> for WorkspaceMetadata {
    type Error = anyhow::Error;

    fn try_from(value: serde_json::Value) -> Result<Self, Self::Error> {
        match value.get("cargo-bazel") {
            Some(value) => {
                serde_json::from_value(value.to_owned()).context("Failed to deserialize json value")
            }
            None => bail!("cargo-bazel workspace metadata not found"),
        }
    }
}

impl WorkspaceMetadata {
    fn new(
        splicing_manifest: &SplicingManifest,
        member_manifests: BTreeMap<&Utf8PathBuf, String>,
    ) -> Result<Self> {
        let mut package_prefixes: BTreeMap<String, String> = member_manifests
            .iter()
            .filter_map(|(original_manifest, cargo_package_name)| {
                let label = match splicing_manifest.manifests.get(*original_manifest) {
                    Some(v) => v,
                    None => return None,
                };

                let package = match label.package() {
                    Some(package) if !package.is_empty() => PathBuf::from(package),
                    Some(_) | None => return None,
                };

                let prefix = package.to_string_lossy().to_string();

                Some((cargo_package_name.clone(), prefix))
            })
            .collect();

        // It is invald for toml maps to use empty strings as keys. In the case
        // the empty key is expected to be the root package. If the root package
        // has a prefix, then all other packages will as well (even if no other
        // manifest represents them). The value is then saved as a separate value
        let workspace_prefix = package_prefixes.remove("");

        let package_prefixes = package_prefixes
            .into_iter()
            .map(|(k, v)| {
                let prefix_path = PathBuf::from(v);
                let prefix = prefix_path.parent().unwrap();
                (k, prefix.to_string_lossy().to_string())
            })
            .collect();

        Ok(Self {
            sources: BTreeMap::new(),
            workspace_prefix,
            package_prefixes,
            tree_metadata: TreeResolverMetadata::new(),
        })
    }

    /// Update an existing Cargo manifest with metadata about registry urls and target
    /// features that are needed in generator steps beyond splicing.
    #[tracing::instrument(skip_all)]
    pub(crate) fn write_registry_urls_and_feature_map(
        cargo: &Cargo,
        lockfile: &cargo_lock::Lockfile,
        resolver_data: TreeResolverMetadata,
        input_manifest_path: &Utf8Path,
        output_manifest_path: &Utf8Path,
    ) -> Result<()> {
        let mut manifest = read_manifest(input_manifest_path)?;

        let mut workspace_metaata = WorkspaceMetadata::try_from(
            manifest
                .workspace
                .as_ref()
                .unwrap()
                .metadata
                .as_ref()
                .unwrap()
                .clone(),
        )?;

        // Locate all packages sourced from a registry
        let pkg_sources: Vec<&cargo_lock::Package> = lockfile
            .packages
            .iter()
            .filter(|pkg| pkg.source.is_some())
            .filter(|pkg| pkg.source.as_ref().unwrap().is_registry())
            .collect();

        // Collect a unique set of index urls
        let index_urls: BTreeSet<(SourceKind, String)> = pkg_sources
            .iter()
            .map(|pkg| {
                let source = pkg.source.as_ref().unwrap();
                (source.kind().clone(), source.url().to_string())
            })
            .collect();

        // Load the cargo config
        let cargo_config = {
            // Note that this path must match the one defined in `splicing::setup_cargo_config`
            let config_path = input_manifest_path
                .parent()
                .unwrap()
                .join(".cargo")
                .join("config.toml");

            if config_path.exists() {
                Some(CargoConfig::try_from_path(config_path.as_std_path())?)
            } else {
                None
            }
        };

        // Load each index for easy access
        let crate_indexes = index_urls
            .into_iter()
            .map(|(source_kind, url)| {
                // Ensure the correct registry is mapped based on the give Cargo config.
                let index_url = if let Some(config) = &cargo_config {
                    config.resolve_replacement_url(&url)?
                } else {
                    &url
                };
                let index = if cargo.use_sparse_registries_for_crates_io()?
                    && index_url == utils::CRATES_IO_INDEX_URL
                {
                    CrateIndexLookup::Http(crates_index::SparseIndex::from_url(
                        "sparse+https://index.crates.io/",
                    )?)
                } else if index_url.starts_with("sparse+") {
                    CrateIndexLookup::Http(crates_index::SparseIndex::from_url(index_url)?)
                } else {
                    match source_kind {
                        SourceKind::Registry => {
                            let index = {
                                // Load the index for the current url
                                let index = crates_index::GitIndex::from_url(index_url)
                                    .with_context(|| {
                                        format!("Failed to load index for url: {index_url}")
                                    })?;

                                // Ensure each index has a valid index config
                                index.index_config().with_context(|| {
                                    format!("`config.json` not found in index: {index_url}")
                                })?;

                                index
                            };
                            CrateIndexLookup::Git(index)
                        }
                        SourceKind::SparseRegistry => {
                            CrateIndexLookup::Http(crates_index::SparseIndex::from_url(
                                format!("sparse+{}", index_url).as_str(),
                            )?)
                        }
                        unknown => {
                            return Err(anyhow!(
                                "'{:?}' crate index type is not supported (caused by '{}')",
                                &unknown,
                                url
                            ));
                        }
                    }
                };
                Ok((url, index))
            })
            .collect::<Result<BTreeMap<String, _>>>()
            .context("Failed to locate crate indexes")?;

        // Get the download URL of each package based on it's registry url.
        let additional_sources = pkg_sources
            .iter()
            .map(|pkg| {
                let source_id = pkg.source.as_ref().unwrap();
                let source_url = source_id.url().to_string();
                let lookup = crate_indexes.get(&source_url).ok_or_else(|| {
                    anyhow!(
                        "Couldn't find crate_index data for SourceID {:?}",
                        source_id
                    )
                })?;
                lookup.get_source_info(pkg).map(|source_info| {
                    (
                        CrateId::new(pkg.name.as_str().to_owned(), pkg.version.clone()),
                        source_info,
                    )
                })
            })
            .collect::<Result<Vec<_>>>()?;

        workspace_metaata
            .sources
            .extend(
                additional_sources
                    .into_iter()
                    .filter_map(|(crate_id, source_info)| {
                        source_info.map(|source_info| (crate_id, source_info))
                    }),
            );
        workspace_metaata.tree_metadata = resolver_data;
        workspace_metaata.inject_into(&mut manifest)?;

        write_root_manifest(output_manifest_path.as_std_path(), manifest)?;

        Ok(())
    }

    fn inject_into(&self, manifest: &mut Manifest) -> Result<()> {
        let metadata_value = toml::Value::try_from(self)?;
        let workspace = manifest.workspace.as_mut().unwrap();

        match &mut workspace.metadata {
            Some(data) => match data.as_table_mut() {
                Some(map) => {
                    map.insert("cargo-bazel".to_owned(), metadata_value);
                }
                None => bail!("The metadata field is always expected to be a table"),
            },
            None => {
                let mut table = toml::map::Map::new();
                table.insert("cargo-bazel".to_owned(), metadata_value);
                workspace.metadata = Some(toml::Value::Table(table))
            }
        }

        Ok(())
    }
}

#[derive(Debug)]
pub(crate) enum SplicedManifest {
    Workspace(Utf8PathBuf),
    Package(Utf8PathBuf),
    MultiPackage(Utf8PathBuf),
}

impl SplicedManifest {
    pub(crate) fn as_path_buf(&self) -> &Utf8PathBuf {
        match self {
            SplicedManifest::Workspace(p) => p,
            SplicedManifest::Package(p) => p,
            SplicedManifest::MultiPackage(p) => p,
        }
    }
}

pub(crate) fn read_manifest(manifest: &Utf8Path) -> Result<Manifest> {
    let content = fs::read_to_string(manifest.as_std_path())?;
    cargo_toml::Manifest::from_str(content.as_str()).context("Failed to deserialize manifest")
}

pub(crate) fn generate_lockfile(
    manifest_path: &SplicedManifest,
    existing_lock: &Option<PathBuf>,
    cargo_bin: Cargo,
    update_request: &Option<CargoUpdateRequest>,
) -> Result<cargo_lock::Lockfile> {
    let manifest_dir = manifest_path
        .as_path_buf()
        .parent()
        .expect("Every manifest should be contained in a parent directory");

    let root_lockfile_path = manifest_dir.join("Cargo.lock");

    // Remove the file so it's not overwitten if it happens to be a symlink.
    if root_lockfile_path.exists() {
        fs::remove_file(&root_lockfile_path)?;
    }

    // Generate the new lockfile
    let lockfile = LockGenerator::new(cargo_bin).generate(
        manifest_path.as_path_buf(),
        existing_lock,
        update_request,
    )?;

    // Write the lockfile to disk
    if !root_lockfile_path.exists() {
        bail!("Failed to generate Cargo.lock file")
    }

    Ok(lockfile)
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn deserialize_splicing_manifest() {
        let runfiles = runfiles::Runfiles::create().unwrap();
        let path = runfiles::rlocation!(
            runfiles,
            "rules_rust/crate_universe/test_data/serialized_configs/splicing_manifest.json"
        )
        .unwrap();

        let content = std::fs::read_to_string(path).unwrap();

        let manifest: SplicingManifest = serde_json::from_str(&content).unwrap();

        // Check manifests
        assert_eq!(
            manifest.manifests,
            BTreeMap::from([
                (
                    Utf8PathBuf::from("${build_workspace_directory}/submod/Cargo.toml"),
                    Label::from_str("//submod:Cargo.toml").unwrap()
                ),
                (
                    Utf8PathBuf::from("${output_base}/external_crate/Cargo.toml"),
                    Label::from_str("@external_crate//:Cargo.toml").unwrap()
                ),
                (
                    Utf8PathBuf::from("/tmp/abs/path/workspace/Cargo.toml"),
                    Label::from_str("//:Cargo.toml").unwrap()
                ),
            ])
        );

        // Check splicing configs
        assert_eq!(manifest.resolver_version, cargo_toml::Resolver::V2);

        // Check packages
        assert_eq!(manifest.direct_packages.len(), 4);
        let package = manifest.direct_packages.get("rand").unwrap();
        assert_eq!(
            package,
            &cargo_toml::DependencyDetail {
                default_features: false,
                features: vec!["small_rng".to_owned()],
                version: Some("0.8.5".to_owned()),
                ..Default::default()
            }
        );
        let package = manifest.direct_packages.get("cfg-if").unwrap();
        assert_eq!(
            package,
            &cargo_toml::DependencyDetail {
                git: Some("https://github.com/rust-lang/cfg-if.git".to_owned()),
                rev: Some("b9c2246a".to_owned()),
                default_features: true,
                ..Default::default()
            }
        );
        let package = manifest.direct_packages.get("log").unwrap();
        assert_eq!(
            package,
            &cargo_toml::DependencyDetail {
                git: Some("https://github.com/rust-lang/log.git".to_owned()),
                branch: Some("master".to_owned()),
                default_features: true,
                ..Default::default()
            }
        );
        let package = manifest.direct_packages.get("cargo_toml").unwrap();
        assert_eq!(
            package,
            &cargo_toml::DependencyDetail {
                git: Some("https://gitlab.com/crates.rs/cargo_toml.git".to_owned()),
                tag: Some("v0.15.2".to_owned()),
                default_features: true,
                ..Default::default()
            }
        );

        // Check cargo config
        assert_eq!(
            manifest.cargo_config,
            Some(Utf8PathBuf::from(
                "/tmp/abs/path/workspace/.cargo/config.toml"
            ))
        );
    }

    #[test]
    fn splicing_manifest_resolve() {
        let runfiles = runfiles::Runfiles::create().unwrap();
        let path = runfiles::rlocation!(
            runfiles,
            "rules_rust/crate_universe/test_data/serialized_configs/splicing_manifest.json"
        )
        .unwrap();

        let content = std::fs::read_to_string(path).unwrap();

        let mut manifest: SplicingManifest = serde_json::from_str(&content).unwrap();
        manifest.cargo_config = Some(Utf8PathBuf::from(
            "${build_workspace_directory}/.cargo/config.toml",
        ));
        manifest = manifest.resolve(
            &PathBuf::from("/tmp/abs/path/workspace"),
            &PathBuf::from("/tmp/output_base"),
        );

        // Check manifests
        assert_eq!(
            manifest.manifests,
            BTreeMap::from([
                (
                    Utf8PathBuf::from("/tmp/abs/path/workspace/submod/Cargo.toml"),
                    Label::from_str("//submod:Cargo.toml").unwrap()
                ),
                (
                    Utf8PathBuf::from("/tmp/output_base/external_crate/Cargo.toml"),
                    Label::from_str("@external_crate//:Cargo.toml").unwrap()
                ),
                (
                    Utf8PathBuf::from("/tmp/abs/path/workspace/Cargo.toml"),
                    Label::from_str("//:Cargo.toml").unwrap()
                ),
            ])
        );

        // Check cargo config
        assert_eq!(
            manifest.cargo_config.unwrap(),
            PathBuf::from("/tmp/abs/path/workspace/.cargo/config.toml"),
        )
    }

    #[test]
    fn splicing_metadata_workspace_path() {
        let runfiles = runfiles::Runfiles::create().unwrap();
        let workspace_manifest_path = runfiles::rlocation!(
            runfiles,
            "rules_rust/crate_universe/test_data/metadata/workspace_path/Cargo.toml"
        )
        .unwrap();
        let workspace_path = workspace_manifest_path.parent().unwrap().to_path_buf();
        let child_a_manifest_path = runfiles::rlocation!(
            runfiles,
            "rules_rust/crate_universe/test_data/metadata/workspace_path/child_a/Cargo.toml"
        )
        .unwrap();
        let child_b_manifest_path = runfiles::rlocation!(
            runfiles,
            "rules_rust/crate_universe/test_data/metadata/workspace_path/child_b/Cargo.toml"
        )
        .unwrap();
        let manifest = SplicingManifest {
            direct_packages: BTreeMap::new(),
            manifests: BTreeMap::from([
                (
                    Utf8PathBuf::try_from(workspace_manifest_path).unwrap(),
                    Label::from_str("//:Cargo.toml").unwrap(),
                ),
                (
                    Utf8PathBuf::try_from(child_a_manifest_path).unwrap(),
                    Label::from_str("//child_a:Cargo.toml").unwrap(),
                ),
                (
                    Utf8PathBuf::try_from(child_b_manifest_path).unwrap(),
                    Label::from_str("//child_b:Cargo.toml").unwrap(),
                ),
            ]),
            cargo_config: None,
            resolver_version: cargo_toml::Resolver::V2,
        };
        let metadata = SplicingMetadata::try_from(manifest).unwrap();
        let metadata = serde_json::to_string(&metadata).unwrap();
        assert!(
            !metadata.contains(workspace_path.to_str().unwrap()),
            "serialized metadata should not contain absolute path"
        );
    }
}
