//! Convert annotated metadata into a renderable context

pub(crate) mod crate_context;
mod platforms;

use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use anyhow::Result;
use serde::{Deserialize, Serialize};

use crate::config::{CrateId, RenderConfig};
use crate::context::platforms::resolve_cfg_platforms;
use crate::lockfile::Digest;
use crate::metadata::{Annotations, Dependency};
use crate::select::Select;
use crate::utils::target_triple::TargetTriple;

pub(crate) use self::crate_context::*;

/// A struct containing information about a Cargo dependency graph in an easily to consume
/// format for rendering reproducible Bazel targets.
#[derive(Debug, Default, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub(crate) struct Context {
    /// The collective checksum of all inputs to the context
    pub(crate) checksum: Option<Digest>,

    /// The collection of all crates that make up the dependency graph
    pub(crate) crates: BTreeMap<CrateId, CrateContext>,

    /// A subset of only crates with binary targets
    pub(crate) binary_crates: BTreeSet<CrateId>,

    /// A subset of workspace members mapping to their workspace
    /// path relative to the workspace root
    pub(crate) workspace_members: BTreeMap<CrateId, String>,

    /// A mapping of `cfg` flags to platform triples supporting the configuration
    pub(crate) conditions: BTreeMap<String, BTreeSet<TargetTriple>>,

    /// A list of crates visible to any bazel module.
    pub(crate) direct_deps: BTreeSet<CrateId>,

    /// A list of crates visible to this bazel module.
    pub(crate) direct_dev_deps: BTreeSet<CrateId>,

    /// A list of `[patch]` entries from the Cargo.lock file which were not used in the resolve.
    // TODO: Remove the serde(default) after this has been released for a few versions.
    // This prevents previous lockfiles (from before this field) from failing to parse with the current version of rules_rust.
    // After we've supported this (so serialised it in lockfiles) for a few versions,
    // we can remove the default fallback because existing lockfiles should have the key present.
    #[serde(default)]
    pub(crate) unused_patches: BTreeSet<cargo_lock::Dependency>,
}

impl Context {
    pub(crate) fn try_from_path<T: AsRef<Path>>(path: T) -> Result<Self> {
        let data = fs::read_to_string(path.as_ref())?;
        Ok(serde_json::from_str(&data)?)
    }

    pub(crate) fn new(annotations: Annotations, sources_are_present: bool) -> anyhow::Result<Self> {
        // Build a map of crate contexts
        let crates: BTreeMap<CrateId, CrateContext> = annotations
            .metadata
            .crates
            .values()
            .map(|annotation| {
                let context = CrateContext::new(
                    annotation,
                    &annotations.metadata.packages,
                    &annotations.lockfile.crates,
                    &annotations.pairred_extras,
                    &annotations.metadata.workspace_metadata.tree_metadata,
                    annotations.config.generate_binaries,
                    annotations.config.generate_build_scripts,
                    sources_are_present,
                )?;
                let id = CrateId::new(context.name.clone(), context.version.clone());
                Ok::<_, anyhow::Error>((id, context))
            })
            .collect::<Result<_, _>>()?;

        // Filter for any crate that contains a binary
        let binary_crates: BTreeSet<CrateId> = crates
            .iter()
            .filter(|(_, ctx)| ctx.targets.iter().any(|t| matches!(t, Rule::Binary(..))))
            // Only consider remote repositories (so non-workspace members).
            .filter(|(_, ctx)| ctx.repository.is_some())
            .map(|(id, _)| id.clone())
            .collect();

        // Given a list of all conditional dependencies, build a set of platform
        // triples which satisfy the conditions.
        let conditions = resolve_cfg_platforms(
            crates.values().collect(),
            &annotations.config.supported_platform_triples,
        )?;

        // Generate a list of all workspace members
        let workspace_members = annotations
            .metadata
            .workspace_members
            .iter()
            .filter_map(|id| {
                let pkg = &annotations.metadata.packages[id];
                let package_path_id = match Self::get_package_path_id(
                    pkg,
                    &annotations.metadata.workspace_root,
                    &annotations.metadata.workspace_metadata.workspace_prefix,
                    &annotations.metadata.workspace_metadata.package_prefixes,
                ) {
                    Ok(id) => id,
                    Err(e) => return Some(Err(e)),
                };
                let crate_id = CrateId::from(pkg);

                // Crates that have repository information are not considered workspace members.
                // The assumpion is that they are "extra workspace members".
                match crates[&crate_id].repository {
                    Some(_) => None,
                    None => Some(Ok((crate_id, package_path_id))),
                }
            })
            .collect::<Result<BTreeMap<CrateId, String>>>()?;

        let add_crate_ids = |crates: &mut BTreeSet<CrateId>,
                             deps: &Select<BTreeSet<Dependency>>| {
            for dep in deps.values() {
                crates.insert(CrateId::from(
                    &annotations.metadata.packages[&dep.package_id],
                ));
            }
        };

        let mut direct_deps: BTreeSet<CrateId> = BTreeSet::new();
        let mut direct_dev_deps: BTreeSet<CrateId> = BTreeSet::new();
        for id in &annotations.metadata.workspace_members {
            let deps = &annotations.metadata.crates[id].deps;
            add_crate_ids(&mut direct_deps, &deps.normal_deps);
            add_crate_ids(&mut direct_deps, &deps.proc_macro_deps);
            add_crate_ids(&mut direct_deps, &deps.build_deps);
            add_crate_ids(&mut direct_deps, &deps.build_link_deps);
            add_crate_ids(&mut direct_deps, &deps.build_proc_macro_deps);
            add_crate_ids(&mut direct_dev_deps, &deps.normal_dev_deps);
            add_crate_ids(&mut direct_dev_deps, &deps.proc_macro_dev_deps);
        }

        let unused_patches = annotations.lockfile.unused_patches;

        Ok(Self {
            checksum: None,
            crates,
            binary_crates,
            workspace_members,
            conditions,
            direct_dev_deps: direct_dev_deps.difference(&direct_deps).cloned().collect(),
            direct_deps,
            unused_patches,
        })
    }

    // A helper function for locating the unique path in a workspace to a workspace member
    fn get_package_path_id(
        package: &cargo_metadata::Package,
        workspace_root: &Path,
        workspace_prefix: &Option<String>,
        package_prefixes: &BTreeMap<String, String>,
    ) -> Result<String> {
        // Locate the package's manifest directory
        let manifest_dir = package
            .manifest_path
            .parent()
            .expect("Every manifest should have a parent")
            .as_std_path();

        // Compare it with the root of the workspace
        let package_path_diff = pathdiff::diff_paths(manifest_dir, workspace_root)
            .expect("Every workspace member's manifest is a child of the workspace root");

        // Ensure the package paths are adjusted in the macros according to the splicing results
        let package_path = match package_prefixes.get(&package.name) {
            // Any package prefix should be absolute and therefore always applied
            Some(prefix) => PathBuf::from(prefix).join(package_path_diff),
            // If no package prefix is present, attempt to apply the workspace prefix
            // since workspace members would not have shown up with their own label
            None => match workspace_prefix {
                Some(prefix) => PathBuf::from(prefix).join(package_path_diff),
                None => package_path_diff,
            },
        };

        // Sanitize the path for increased consistency
        let package_path_id = package_path
            .display()
            .to_string()
            .replace('\\', "/")
            .trim_matches('/')
            .to_owned();

        Ok(package_path_id)
    }

    /// Create a set of all direct dependencies of workspace member crates.
    pub(crate) fn workspace_member_deps(&self) -> BTreeSet<CrateDependency> {
        self.workspace_members
            .keys()
            .map(move |id| &self.crates[id])
            .flat_map(|ctx| {
                IntoIterator::into_iter([
                    &ctx.common_attrs.deps,
                    &ctx.common_attrs.deps_dev,
                    &ctx.common_attrs.proc_macro_deps,
                    &ctx.common_attrs.proc_macro_deps_dev,
                ])
                .flat_map(|deps| deps.values())
            })
            .collect()
    }

    pub(crate) fn has_duplicate_workspace_member_dep(&self, dep: &CrateDependency) -> bool {
        1 < self
            .workspace_member_deps()
            .into_iter()
            .filter(|check| check.id.name == dep.id.name && check.alias == dep.alias)
            .count()
    }

    pub(crate) fn has_duplicate_binary_crate(&self, bin: &CrateId) -> bool {
        1 < self
            .binary_crates
            .iter()
            .filter(|check| check.name == bin.name)
            .count()
    }
}

/// All information needed to render a BUILD file for a single crate.
#[derive(Debug, Serialize, Deserialize)]
pub struct SingleBuildFileRenderContext {
    /// The RenderConfig.
    pub(crate) config: Arc<RenderConfig>,

    /// See crate::config::Config.supported_platform_triples.
    pub(crate) supported_platform_triples: Arc<BTreeSet<TargetTriple>>,

    /// See Context::conditions.
    pub(crate) platform_conditions: Arc<BTreeMap<String, BTreeSet<TargetTriple>>>,

    /// The CrateContext for the crate being rendered.
    pub(crate) crate_context: Arc<CrateContext>,
}

#[cfg(test)]
mod test {
    use super::*;
    use camino::Utf8Path;
    use semver::Version;

    use crate::config::Config;

    fn mock_context_common() -> Context {
        let annotations = Annotations::new(
            crate::test::metadata::common(),
            crate::test::lockfile::common(),
            Config::default(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();

        Context::new(annotations, false).unwrap()
    }

    fn mock_context_aliases() -> Context {
        let annotations = Annotations::new(
            crate::test::metadata::alias(),
            crate::test::lockfile::alias(),
            Config::default(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();

        Context::new(annotations, false).unwrap()
    }

    #[test]
    fn workspace_member_deps_collection() {
        let context = mock_context_common();
        let workspace_member_deps = context.workspace_member_deps();

        assert_eq! {
            workspace_member_deps
                .iter()
                .map(|dep| (&dep.id, context.has_duplicate_workspace_member_dep(dep)))
                .collect::<Vec<_>>(),
            [
                (&CrateId::new("bitflags".to_owned(), Version::new(1, 3, 2)), false),
                (&CrateId::new("cfg-if".to_owned(), Version::new(1, 0, 0)), false),
            ],
        }
    }

    #[test]
    fn workspace_member_deps_with_aliases() {
        let context = mock_context_aliases();
        let workspace_member_deps = context.workspace_member_deps();

        assert_eq! {
            workspace_member_deps
                .iter()
                .map(|dep| (&dep.id, context.has_duplicate_workspace_member_dep(dep)))
                .collect::<Vec<_>>(),
            [
                (&CrateId::new("log".to_owned(), Version::new(0, 3, 9)), false),
                (&CrateId::new("log".to_owned(), Version::new(0, 4, 21)), false),
                (&CrateId::new("names".to_owned(), Version::parse("0.12.1-dev").unwrap()), false),
                (&CrateId::new("names".to_owned(), Version::new(0, 13, 0)), false),
                (&CrateId::new("surrealdb".to_owned(), Version::new(1, 3, 1)), false),
                (&CrateId::new("value-bag".to_owned(), Version::parse("1.0.0-alpha.7").unwrap()), false),
            ],
        }
    }

    #[test]
    fn serialization() {
        let context = mock_context_aliases();

        // Serialize and deserialize the context object
        let json_text = serde_json::to_string(&context).unwrap();
        let deserialized_context: Context = serde_json::from_str(&json_text).unwrap();

        // The data should be identical
        assert_eq!(context, deserialized_context);
    }
}
