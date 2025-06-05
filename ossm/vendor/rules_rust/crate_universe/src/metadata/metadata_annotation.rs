//! Collect and store information from Cargo metadata specific to Bazel's needs

use std::collections::{BTreeMap, BTreeSet};
use std::path::PathBuf;

use anyhow::{bail, Result};
use camino::{Utf8Path, Utf8PathBuf};
use cargo_metadata::{Node, Package, PackageId};
use hex::ToHex;
use serde::{Deserialize, Serialize};

use crate::config::{Commitish, Config, CrateAnnotations, CrateId};
use crate::metadata::dependency::DependencySet;
use crate::metadata::TreeResolverMetadata;
use crate::splicing::{SourceInfo, WorkspaceMetadata};

pub(crate) type CargoMetadata = cargo_metadata::Metadata;
pub(crate) type CargoLockfile = cargo_lock::Lockfile;

/// Additional information about a crate relative to other crates in a dependency graph.
#[derive(Debug, Serialize, Deserialize)]
pub(crate) struct CrateAnnotation {
    /// The crate's node in the Cargo "resolve" graph.
    pub(crate) node: Node,

    /// The crate's sorted dependencies.
    pub(crate) deps: DependencySet,
}

/// Additional information about a Cargo workspace's metadata.
#[derive(Debug, Default, Serialize, Deserialize)]
pub(crate) struct MetadataAnnotation {
    /// All packages found within the Cargo metadata
    pub(crate) packages: BTreeMap<PackageId, Package>,

    /// All [CrateAnnotation]s for all packages
    pub(crate) crates: BTreeMap<PackageId, CrateAnnotation>,

    /// All packages that are workspace members
    pub(crate) workspace_members: BTreeSet<PackageId>,

    /// The path to the directory containing the Cargo workspace that produced the metadata.
    pub(crate) workspace_root: PathBuf,

    /// Information on the Cargo workspace.
    pub(crate) workspace_metadata: WorkspaceMetadata,
}

impl MetadataAnnotation {
    pub(crate) fn new(metadata: CargoMetadata) -> MetadataAnnotation {
        // UNWRAP: The workspace metadata should be written by a controlled process. This should not return a result
        let workspace_metadata = find_workspace_metadata(&metadata).unwrap_or_default();

        let resolve = metadata
            .resolve
            .as_ref()
            .expect("The metadata provided requires a resolve graph")
            .clone();

        let is_node_workspace_member = |node: &Node, metadata: &CargoMetadata| -> bool {
            metadata.workspace_members.iter().any(|pkg| pkg == &node.id)
        };

        let workspace_members: BTreeSet<PackageId> = resolve
            .nodes
            .iter()
            .filter(|node| is_node_workspace_member(node, &metadata))
            .map(|node| node.id.clone())
            .collect();

        let crates = resolve
            .nodes
            .iter()
            .map(|node| {
                (
                    node.id.clone(),
                    Self::annotate_crate(
                        node.clone(),
                        &metadata,
                        &workspace_metadata.tree_metadata,
                    ),
                )
            })
            .collect();

        let packages = metadata
            .packages
            .into_iter()
            .map(|pkg| (pkg.id.clone(), pkg))
            .collect();

        MetadataAnnotation {
            packages,
            crates,
            workspace_members,
            workspace_root: PathBuf::from(metadata.workspace_root.as_std_path()),
            workspace_metadata,
        }
    }

    fn annotate_crate(
        node: Node,
        metadata: &CargoMetadata,
        resolver_data: &TreeResolverMetadata,
    ) -> CrateAnnotation {
        // Gather all dependencies
        let deps = DependencySet::new_for_node(&node, metadata, resolver_data);

        CrateAnnotation { node, deps }
    }
}

/// Additional information about how and where to acquire a crate's source code from.
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub(crate) enum SourceAnnotation {
    Git {
        /// The Git url where to clone the source from.
        remote: String,

        /// The revision information for the git repository. This is used for
        /// [git_repository::commit](https://docs.bazel.build/versions/main/repo/git.html#git_repository-commit),
        /// [git_repository::tag](https://docs.bazel.build/versions/main/repo/git.html#git_repository-tag), or
        /// [git_repository::branch](https://docs.bazel.build/versions/main/repo/git.html#git_repository-branch).
        commitish: Commitish,

        /// See [git_repository::shallow_since](https://docs.bazel.build/versions/main/repo/git.html#git_repository-shallow_since)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        shallow_since: Option<String>,

        /// See [git_repository::strip_prefix](https://docs.bazel.build/versions/main/repo/git.html#git_repository-strip_prefix)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        strip_prefix: Option<String>,

        /// See [git_repository::patch_args](https://docs.bazel.build/versions/main/repo/git.html#git_repository-patch_args)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        patch_args: Option<Vec<String>>,

        /// See [git_repository::patch_tool](https://docs.bazel.build/versions/main/repo/git.html#git_repository-patch_tool)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        patch_tool: Option<String>,

        /// See [git_repository::patches](https://docs.bazel.build/versions/main/repo/git.html#git_repository-patches)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        patches: Option<BTreeSet<String>>,
    },
    Http {
        /// See [http_archive::url](https://docs.bazel.build/versions/main/repo/http.html#http_archive-url)
        url: String,

        /// See [http_archive::sha256](https://docs.bazel.build/versions/main/repo/http.html#http_archive-sha256)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        sha256: Option<String>,

        /// See [http_archive::patch_args](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_args)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        patch_args: Option<Vec<String>>,

        /// See [http_archive::patch_tool](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_tool)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        patch_tool: Option<String>,

        /// See [http_archive::patches](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patches)
        #[serde(default, skip_serializing_if = "Option::is_none")]
        patches: Option<BTreeSet<String>>,
    },
    Path {
        path: Utf8PathBuf,
    },
}

/// Additional information related to [Cargo.lock](https://doc.rust-lang.org/cargo/guide/cargo-toml-vs-cargo-lock.html)
/// data used for improved determinism.
#[derive(Debug, Default, PartialEq, Eq, PartialOrd, Ord, Deserialize, Serialize)]
pub(crate) struct LockfileAnnotation {
    /// A mapping of crates/packages to additional source (network location) information.
    pub(crate) crates: BTreeMap<PackageId, SourceAnnotation>,

    /// A list of `[patch]` entries from the Cargo.lock file which were not used in the resolve.
    pub(crate) unused_patches: BTreeSet<cargo_lock::Dependency>,
}

impl LockfileAnnotation {
    pub(crate) fn new(
        lockfile: CargoLockfile,
        metadata: &CargoMetadata,
        nonhermetic_root_bazel_workspace_dir: &Utf8Path,
    ) -> Result<Self> {
        let workspace_metadata = find_workspace_metadata(metadata).unwrap_or_default();

        let nodes: Vec<&Node> = metadata
            .resolve
            .as_ref()
            .expect("Metadata is expected to have a resolve graph")
            .nodes
            .iter()
            .filter(|node| !is_workspace_member(&node.id, metadata))
            .collect();

        // Produce source annotations for each crate in the resolve graph
        let crates = nodes
            .iter()
            .map(|node| {
                Ok((
                    node.id.clone(),
                    Self::collect_source_annotations(
                        node,
                        metadata,
                        &lockfile,
                        &workspace_metadata,
                        nonhermetic_root_bazel_workspace_dir,
                    )?,
                ))
            })
            .collect::<Result<BTreeMap<PackageId, SourceAnnotation>>>()?;

        let unused_patches = lockfile.patch.unused.into_iter().collect();

        Ok(Self {
            crates,
            unused_patches,
        })
    }

    /// Resolve all URLs and checksum-like data for each package
    fn collect_source_annotations(
        node: &Node,
        metadata: &CargoMetadata,
        lockfile: &CargoLockfile,
        workspace_metadata: &WorkspaceMetadata,
        nonhermetic_root_bazel_workspace_dir: &Utf8Path,
    ) -> Result<SourceAnnotation> {
        let pkg = &metadata[&node.id];

        // Locate the matching lock package for the current crate
        let lock_pkg = match cargo_meta_pkg_to_locked_pkg(pkg, &lockfile.packages) {
            Some(lock_pkg) => lock_pkg,
            None => bail!(
                "Could not find lockfile entry matching metadata package '{}'",
                pkg.name
            ),
        };

        // Check for spliced information about a crate's network source.
        let spliced_source_info = Self::find_source_annotation(lock_pkg, workspace_metadata);

        // Parse its source info. The check above should prevent a panic
        let source = match lock_pkg.source.as_ref() {
            Some(source) => source,
            None => match spliced_source_info {
                Some(info) => {
                    return Ok(SourceAnnotation::Http {
                        url: info.url,
                        sha256: Some(info.sha256),
                        patch_args: None,
                        patch_tool: None,
                        patches: None,
                    })
                }
                None => {
                    // Test for path deps that may look something like path+file:///var/folders/xs/1d3z0l8977v1_kk4r3_py4l80000gn/T/tmp.AIICMiDy#lazy_static@1.5.0
                    if let Some(path_with_suffix) = node.id.repr.strip_prefix("path+file://") {
                        if let Some((path_in_lockfile, _suffix)) = path_with_suffix.rsplit_once('#')
                        {
                            let path = match Utf8Path::new(path_in_lockfile)
                                .strip_prefix(&metadata.workspace_root)
                            {
                                Ok(suffix) => {
                                    // Replace path within our temporary cargo workspace we ran `cargo metadata`` in with path within the actual Bazel workspace.
                                    // This replacement allows in-repo patches sections to work as intended using local_crate_mirror.
                                    let mut new_path =
                                        nonhermetic_root_bazel_workspace_dir.to_owned();
                                    if let Some(prefix) =
                                        workspace_metadata.workspace_prefix.as_ref()
                                    {
                                        new_path.push(prefix);
                                    }
                                    new_path.push(suffix);
                                    new_path
                                }
                                Err(_) => Utf8PathBuf::from(path_in_lockfile),
                            };
                            return Ok(SourceAnnotation::Path { path });
                        }
                    }
                    bail!(
                        "The package '{:?} {:?}: {:?}' has no source info so no annotation can be made",
                        lock_pkg.name,
                        lock_pkg.version,
                        node.id.repr
                    );
                }
            },
        };

        // Handle any git repositories
        if let Some(git_ref) = source.git_reference() {
            let strip_prefix = Self::extract_git_strip_prefix(pkg)?;

            return Ok(SourceAnnotation::Git {
                remote: source.url().to_string(),
                commitish: source
                    .precise()
                    .map(|rev| Commitish::Rev(rev.to_string()))
                    .unwrap_or(Commitish::from(git_ref.clone())),
                shallow_since: None,
                strip_prefix,
                patch_args: None,
                patch_tool: None,
                patches: None,
            });
        }

        // One of the last things that should be checked is the spliced source information as
        // other sources may more accurately represent where a crate should be downloaded.
        if let Some(info) = spliced_source_info {
            return Ok(SourceAnnotation::Http {
                url: info.url,
                sha256: Some(info.sha256),
                patch_args: None,
                patch_tool: None,
                patches: None,
            });
        }

        // Finally, In the event that no spliced source information was included in the
        // metadata the raw source info is used for registry crates and `crates.io` is
        // assumed to be the source.
        if source.is_registry() {
            // source url
            return Ok(SourceAnnotation::Http {
                url: format!(
                    "https://static.crates.io/crates/{}/{}/download",
                    lock_pkg.name, lock_pkg.version
                ),
                sha256: lock_pkg
                    .checksum
                    .as_ref()
                    .and_then(|sum| {
                        if sum.is_sha256() {
                            sum.as_sha256()
                        } else {
                            None
                        }
                    })
                    .map(|sum| sum.encode_hex::<String>()),
                patch_args: None,
                patch_tool: None,
                patches: None,
            });
        }

        bail!(
            "Unable to determine source annotation for '{:?} {:?}",
            lock_pkg.name,
            lock_pkg.version
        )
    }

    fn find_source_annotation(
        package: &cargo_lock::Package,
        metadata: &WorkspaceMetadata,
    ) -> Option<SourceInfo> {
        let crate_id = CrateId::new(package.name.to_string(), package.version.clone());
        metadata.sources.get(&crate_id).cloned()
    }

    fn extract_git_strip_prefix(pkg: &Package) -> Result<Option<String>> {
        // {CARGO_HOME}/git/checkouts/name-hash/short-sha/[strip_prefix...]/Cargo.toml
        let components = pkg
            .manifest_path
            .components()
            .map(|v| v.to_string())
            .collect::<Vec<_>>();
        for (i, _) in components.iter().enumerate() {
            let possible_components = &components[i..];
            if possible_components.len() < 5 {
                continue;
            }
            if possible_components[0] != "git"
                || possible_components[1] != "checkouts"
                || possible_components[possible_components.len() - 1] != "Cargo.toml"
            {
                continue;
            }
            if possible_components.len() == 5 {
                return Ok(None);
            }
            return Ok(Some(
                possible_components[4..(possible_components.len() - 1)].join("/"),
            ));
        }
        bail!("Expected git package to have a manifest path of pattern {{CARGO_HOME}}/git/checkouts/[name]-[hash]/[short-sha]/.../Cargo.toml but {:?} had manifest path {}", pkg.id, pkg.manifest_path);
    }
}

/// A pairing of a crate's package identifier to its annotations.
#[derive(Debug)]
pub(crate) struct PairedExtras {
    /// The crate's package identifier
    pub(crate) package_id: cargo_metadata::PackageId,

    /// The crate's annotations
    pub(crate) crate_extra: CrateAnnotations,
}

/// A collection of data which has been processed for optimal use in generating Bazel targets.
#[derive(Debug, Default)]
pub(crate) struct Annotations {
    /// Annotated Cargo metadata
    pub(crate) metadata: MetadataAnnotation,

    /// Annotated Cargo lockfile
    pub(crate) lockfile: LockfileAnnotation,

    /// The current workspace's configuration settings
    pub(crate) config: Config,

    /// Pairred crate annotations
    pub(crate) pairred_extras: BTreeMap<CrateId, PairedExtras>,
}

impl Annotations {
    pub(crate) fn new(
        cargo_metadata: CargoMetadata,
        cargo_lockfile: CargoLockfile,
        config: Config,
        nonhermetic_root_bazel_workspace_dir: &Utf8Path,
    ) -> Result<Self> {
        let lockfile_annotation = LockfileAnnotation::new(
            cargo_lockfile,
            &cargo_metadata,
            nonhermetic_root_bazel_workspace_dir,
        )?;

        // Annotate the cargo metadata
        let metadata_annotation = MetadataAnnotation::new(cargo_metadata);

        let mut unused_extra_annotations = config.annotations.clone();

        // Ensure each override matches a particular package
        let pairred_extras = metadata_annotation
            .packages
            .iter()
            .filter_map(|(pkg_id, pkg)| {
                let mut crate_extra: CrateAnnotations = config
                    .annotations
                    .iter()
                    .filter(|(id, _)| id.matches(pkg))
                    .map(|(id, extra)| {
                        // Mark that an annotation has been consumed
                        unused_extra_annotations.remove(id);

                        // Filter out the annotation
                        extra
                    })
                    .cloned()
                    .sum();

                crate_extra.apply_defaults_from_package_metadata(&pkg.metadata);

                if crate_extra == CrateAnnotations::default() {
                    None
                } else {
                    Some((
                        CrateId::new(pkg.name.clone(), pkg.version.clone()),
                        PairedExtras {
                            package_id: pkg_id.clone(),
                            crate_extra,
                        },
                    ))
                }
            })
            .collect();

        // Alert on any unused annotations
        if !unused_extra_annotations.is_empty() {
            bail!(
                "Unused annotations were provided. Please remove them: {:?}",
                unused_extra_annotations.keys()
            );
        }

        // Annotate metadata
        Ok(Annotations {
            metadata: metadata_annotation,
            lockfile: lockfile_annotation,
            config,
            pairred_extras,
        })
    }
}

fn find_workspace_metadata(cargo_metadata: &CargoMetadata) -> Option<WorkspaceMetadata> {
    WorkspaceMetadata::try_from(cargo_metadata.workspace_metadata.clone()).ok()
}

/// Determines whether or not a package is a workspace member. This follows
/// the Cargo definition of a workspace memeber with one exception where
/// "extra workspace members" are *not* treated as workspace members
fn is_workspace_member(id: &PackageId, cargo_metadata: &CargoMetadata) -> bool {
    if cargo_metadata.workspace_members.contains(id) {
        if let Some(data) = find_workspace_metadata(cargo_metadata) {
            let pkg = &cargo_metadata[id];
            let crate_id = CrateId::new(pkg.name.clone(), pkg.version.clone());

            !data.sources.contains_key(&crate_id)
        } else {
            true
        }
    } else {
        false
    }
}

/// Match a [cargo_metadata::Package] to a [cargo_lock::Package].
fn cargo_meta_pkg_to_locked_pkg<'a>(
    pkg: &Package,
    lock_packages: &'a [cargo_lock::Package],
) -> Option<&'a cargo_lock::Package> {
    lock_packages
        .iter()
        .find(|lock_pkg| lock_pkg.name.as_str() == pkg.name && lock_pkg.version == pkg.version)
}

#[cfg(test)]
mod test {
    use super::*;

    use semver::Version;
    use serde_json::json;

    use crate::config::CrateNameAndVersionReq;
    use crate::metadata::CargoTreeEntry;
    use crate::select::Select;
    use crate::test::*;

    #[test]
    fn test_cargo_meta_pkg_to_locked_pkg() {
        let pkg = mock_cargo_metadata_package();
        let lock_pkg = mock_cargo_lock_package();

        assert!(cargo_meta_pkg_to_locked_pkg(&pkg, &vec![lock_pkg]).is_some())
    }

    #[test]
    fn annotate_metadata_with_aliases() {
        let annotations = MetadataAnnotation::new(test::metadata::alias());
        let log_crates: BTreeMap<&PackageId, &CrateAnnotation> = annotations
            .crates
            .iter()
            .filter(|(id, _)| {
                let pkg = &annotations.packages[*id];
                pkg.name == "log"
            })
            .collect();

        assert_eq!(log_crates.len(), 2);
    }

    #[test]
    fn annotate_lockfile_with_aliases() {
        LockfileAnnotation::new(
            test::lockfile::alias(),
            &test::metadata::alias(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();
    }

    #[test]
    fn annotate_metadata_with_build_scripts() {
        MetadataAnnotation::new(test::metadata::build_scripts());
    }

    #[test]
    fn annotate_lockfile_with_build_scripts() {
        LockfileAnnotation::new(
            test::lockfile::build_scripts(),
            &test::metadata::build_scripts(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();
    }

    #[test]
    fn annotate_lockfile_with_no_deps() {
        LockfileAnnotation::new(
            test::lockfile::no_deps(),
            &test::metadata::no_deps(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();
    }

    #[test]
    fn detects_strip_prefix_for_git_repo() {
        let crates = LockfileAnnotation::new(
            test::lockfile::git_repos(),
            &test::metadata::git_repos(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap()
        .crates;
        let tracing_core = crates
            .iter()
            .find(|(k, _)| k.repr.contains("#tracing-core@"))
            .map(|(_, v)| v)
            .unwrap();
        match tracing_core {
            SourceAnnotation::Git {
                strip_prefix: Some(strip_prefix),
                ..
            } if strip_prefix == "tracing-core" => {
                // Matched correctly.
            }
            other => {
                panic!("Wanted SourceAnnotation::Git with strip_prefix == Some(\"tracing-core\"), got: {:?}", other);
            }
        }
    }

    #[test]
    fn resolves_commit_from_branches_and_tags() {
        let crates = LockfileAnnotation::new(
            test::lockfile::git_repos(),
            &test::metadata::git_repos(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap()
        .crates;

        let package_id = PackageId {
            repr: "git+https://github.com/tokio-rs/tracing.git?branch=master#tracing@0.2.0".into(),
        };
        let annotation = crates.get(&package_id).unwrap();

        let commitish = match annotation {
            SourceAnnotation::Git { commitish, .. } => commitish,
            _ => panic!("Unexpected annotation type"),
        };

        assert_eq!(
            *commitish,
            Commitish::Rev("1e09e50e8d15580b5929adbade9c782a6833e4a0".into())
        );
    }

    #[test]
    fn detect_unused_annotation() {
        // Create a config with some random annotation
        let mut config = Config::default();
        config.annotations.insert(
            CrateNameAndVersionReq::new("mock-crate".to_owned(), "0.1.0".parse().unwrap()),
            CrateAnnotations::default(),
        );

        let result = Annotations::new(
            test::metadata::no_deps(),
            test::lockfile::no_deps(),
            config,
            Utf8Path::new("/tmp/bazelworkspace"),
        );
        assert!(result.is_err());

        let result_str = format!("{result:?}");
        assert!(result_str.contains("Unused annotations were provided. Please remove them"));
        assert!(result_str.contains("mock-crate"));
    }

    #[test]
    fn defaults_from_package_metadata() {
        let crate_id = CrateId::new(
            "has_package_metadata".to_owned(),
            semver::Version::new(0, 0, 0),
        );
        let crate_name_and_version_req = CrateNameAndVersionReq::new(
            "has_package_metadata".to_owned(),
            "0.0.0".parse().unwrap(),
        );
        let annotations = CrateAnnotations {
            rustc_env: Some(Select::from_value(BTreeMap::from([(
                "BAR".to_owned(),
                "bar is set".to_owned(),
            )]))),
            ..CrateAnnotations::default()
        };

        let mut config = Config::default();
        config
            .annotations
            .insert(crate_name_and_version_req, annotations.clone());

        // Combine the above annotations with default values provided by the
        // crate author in package metadata.
        let combined_annotations = Annotations::new(
            test::metadata::has_package_metadata(),
            test::lockfile::has_package_metadata(),
            config,
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();

        let extras = &combined_annotations.pairred_extras[&crate_id].crate_extra;
        let expected = CrateAnnotations {
            // This comes from has_package_metadata's [package.metadata.bazel].
            additive_build_file_content: Some("genrule(**kwargs)\n".to_owned()),
            // The package metadata defines a default rustc_env containing FOO,
            // but it is superseded by a rustc_env annotation containing only
            // BAR. These dictionaries are intentionally not merged together.
            ..annotations
        };
        assert_eq!(*extras, expected);
    }

    #[test]
    fn test_find_workspace_metadata() {
        let mut metadata = metadata::common();
        metadata.workspace_metadata = json!({
            "cargo-bazel": {
            "package_prefixes": {},
            "sources": {},
            "tree_metadata": {
                "bitflags 1.3.2": {
                    "common": {
                        "features": [
                            "default",
                        ],
                    },
                    "selects": {
                        "x86_64-unknown-linux-gnu": {
                            "features": [
                                "std",
                            ],
                            "deps": [
                                "libc 1.2.3",
                            ],
                        },
                    }
                }
            },
        }
        });

        let mut select = Select::new();
        select.insert(
            CargoTreeEntry {
                features: BTreeSet::from(["default".to_owned()]),
                deps: BTreeSet::new(),
            },
            None,
        );
        select.insert(
            CargoTreeEntry {
                features: BTreeSet::from(["std".to_owned()]),
                deps: BTreeSet::from([CrateId::new("libc".to_owned(), Version::new(1, 2, 3))]),
            },
            Some("x86_64-unknown-linux-gnu".to_owned()),
        );
        let expected = TreeResolverMetadata::from([(
            CrateId::new("bitflags".to_owned(), Version::new(1, 3, 2)),
            select,
        )]);

        let result = find_workspace_metadata(&metadata).unwrap();

        assert_eq!(expected, result.tree_metadata);
    }
}
