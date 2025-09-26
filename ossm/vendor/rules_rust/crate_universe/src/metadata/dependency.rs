//! Gathering dependencies is the largest part of annotating.

use std::collections::{BTreeMap, BTreeSet};

use anyhow::{bail, Result};
use cargo_metadata::{
    DependencyKind, Metadata as CargoMetadata, Node, NodeDep, Package, PackageId, Target,
};
use cargo_platform::Platform;
use serde::{Deserialize, Serialize};

use crate::config::CrateId;
use crate::metadata::TreeResolverMetadata;
use crate::select::Select;
use crate::utils::sanitize_module_name;

/// A representation of a crate dependency
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub(crate) struct Dependency {
    /// The PackageId of the target
    pub(crate) package_id: PackageId,

    /// The library target name of the dependency.
    pub(crate) target_name: String,

    /// The alias for the dependency from the perspective of the current package
    pub(crate) alias: Option<String>,
}

/// A collection of [Dependency]s sorted by dependency kind.
#[derive(Debug, Default, Serialize, Deserialize)]
pub(crate) struct DependencySet {
    pub(crate) normal_deps: Select<BTreeSet<Dependency>>,
    pub(crate) normal_dev_deps: Select<BTreeSet<Dependency>>,
    pub(crate) proc_macro_deps: Select<BTreeSet<Dependency>>,
    pub(crate) proc_macro_dev_deps: Select<BTreeSet<Dependency>>,
    pub(crate) build_deps: Select<BTreeSet<Dependency>>,
    pub(crate) build_link_deps: Select<BTreeSet<Dependency>>,
    pub(crate) build_proc_macro_deps: Select<BTreeSet<Dependency>>,
}

impl DependencySet {
    /// Collect all dependencies for a given node in the resolve graph.
    pub(crate) fn new_for_node(
        node: &Node,
        metadata: &CargoMetadata,
        resolver_data: &TreeResolverMetadata,
    ) -> Self {
        // Build a dep tree mapping that's easily indexable via `cargo_metadata::PackageId`
        let dep_tree: BTreeMap<CrateId, Select<BTreeSet<CrateId>>> = resolver_data
            .iter()
            .map(|(id, tree_data)| {
                let mut select = Select::new();
                for (config, data) in tree_data.items() {
                    for dep in data.deps {
                        select.insert(dep, config.clone());
                    }
                }
                (id.clone(), select)
            })
            .collect();

        let crate_id = {
            let package = &metadata[&node.id];
            CrateId::from(package)
        };

        let (normal_dev_deps, normal_deps) = {
            let (dev, normal) = node
                .deps
                .iter()
                // Do not track workspace members as dependencies. Users are expected to maintain those connections
                .filter(|dep| !is_workspace_member(dep, metadata))
                .filter(|dep| is_lib_package(&metadata[&dep.pkg]))
                .filter(|dep| is_normal_dependency(dep) || is_dev_dependency(dep))
                .partition(|dep| is_dev_dependency(dep));

            (
                collect_deps_selectable(
                    node,
                    dev,
                    metadata,
                    DependencyKind::Development,
                    dep_tree.get(&crate_id),
                ),
                collect_deps_selectable(
                    node,
                    normal,
                    metadata,
                    DependencyKind::Normal,
                    dep_tree.get(&crate_id),
                ),
            )
        };

        let (proc_macro_dev_deps, proc_macro_deps) = {
            let (dev, normal) = node
                .deps
                .iter()
                // Do not track workspace members as dependencies. Users are expected to maintain those connections
                .filter(|dep| !is_workspace_member(dep, metadata))
                .filter(|dep| is_proc_macro_package(&metadata[&dep.pkg]))
                .filter(|dep| is_normal_dependency(dep) || is_dev_dependency(dep))
                .partition(|dep| is_dev_dependency(dep));

            (
                collect_deps_selectable(
                    node,
                    dev,
                    metadata,
                    DependencyKind::Development,
                    dep_tree.get(&crate_id),
                ),
                collect_deps_selectable(
                    node,
                    normal,
                    metadata,
                    DependencyKind::Normal,
                    dep_tree.get(&crate_id),
                ),
            )
        };

        // For rules on build script dependencies see:
        //  https://doc.rust-lang.org/cargo/reference/build-scripts.html#build-dependencies
        let (build_proc_macro_deps, build_deps) = {
            let (proc_macro, normal) = node
                .deps
                .iter()
                // Do not track workspace members as dependencies. Users are expected to maintain those connections
                .filter(|dep| !is_workspace_member(dep, metadata))
                .filter(|dep| is_build_dependency(dep))
                .filter(|dep| !is_dev_dependency(dep))
                .partition(|dep| is_proc_macro_package(&metadata[&dep.pkg]));

            (
                collect_deps_selectable(
                    node,
                    proc_macro,
                    metadata,
                    DependencyKind::Build,
                    dep_tree.get(&crate_id),
                ),
                collect_deps_selectable(
                    node,
                    normal,
                    metadata,
                    DependencyKind::Build,
                    dep_tree.get(&crate_id),
                ),
            )
        };

        // packages with the `links` property follow slightly different rules than other
        // dependencies. These packages provide zero or more environment variables to the build
        // script's of packages that directly (non-transitively) depend on these packages. Note that
        // dependency specifically means of the package (`dependencies`), and not of the build
        // script (`build-dependencies`).
        // https://doc.rust-lang.org/cargo/reference/build-scripts.html#the-links-manifest-key
        // https://doc.rust-lang.org/cargo/reference/build-scripts.html#-sys-packages
        // https://doc.rust-lang.org/cargo/reference/build-script-examples.html#using-another-sys-crate
        let mut build_link_deps: Select<BTreeSet<Dependency>> = Select::default();
        for (configuration, dependency) in normal_deps
            .items()
            .into_iter()
            .filter(|(_, dependency)| metadata[&dependency.package_id].links.is_some())
        {
            // Add any normal dependency to build dependencies that are associated `*-sys` crates
            build_link_deps.insert(dependency.clone(), configuration.clone());
        }

        Self {
            normal_deps,
            normal_dev_deps,
            proc_macro_deps,
            proc_macro_dev_deps,
            build_deps,
            build_link_deps,
            build_proc_macro_deps,
        }
    }
}

/// For details on optional dependencies see [the Rust docs](https://doc.rust-lang.org/cargo/reference/features.html#optional-dependencies).
fn is_optional_dependency(
    parent: &Node,
    dep: &NodeDep,
    target: Option<&Platform>,
    metadata: &CargoMetadata,
    kind: DependencyKind,
) -> bool {
    let pkg = &metadata[&parent.id];

    pkg.dependencies
        .iter()
        .filter(|&d| d.kind == kind)
        .filter(|&d| d.target.as_ref() == target)
        .filter(|&d| d.optional)
        .any(|d| sanitize_module_name(d.rename.as_ref().unwrap_or(&d.name)) == dep.name)
}

fn collect_deps_selectable(
    node: &Node,
    deps: Vec<&NodeDep>,
    metadata: &cargo_metadata::Metadata,
    kind: DependencyKind,
    tree_data: Option<&Select<BTreeSet<CrateId>>>,
) -> Select<BTreeSet<Dependency>> {
    let mut select: Select<BTreeSet<Dependency>> = Select::default();

    // Unfortunately, Cargo metadata is not as accurate as it could be due
    // to the lack of `resolver = 2` support in the `cargo metadata` subcommand.
    // To ensure accurate dependencies are determined, metadata is only used to
    // determine the general list of dependencies but any one of them can be demoted
    // to a platform specific dep depending on the `cargo tree` data provided.
    //
    // For more details see: https://github.com/rust-lang/cargo/issues/9863
    for dep in deps.into_iter() {
        let dep_pkg = &metadata[&dep.pkg];
        let target_name = get_library_target_name(dep_pkg, &dep.name)
            .expect("Nodes Dependencies are expected to exclusively be library-like targets");
        let alias = get_target_alias(&dep.name, dep_pkg);
        let crate_id = CrateId::from(dep_pkg);

        for kind_info in &dep.dep_kinds {
            if kind_info.kind != kind {
                continue;
            }

            // For optional dependencies, use the Feature tree resolver data to determine
            // if or how the dependency should be added.
            if is_optional_dependency(node, dep, kind_info.target.as_ref(), metadata, kind) {
                // Collect the dependency from any configuration.
                if let Some(data) = tree_data {
                    for (config, tree_dep) in data.items() {
                        if crate_id == tree_dep {
                            let dependency = Dependency {
                                package_id: dep.pkg.clone(),
                                target_name: target_name.clone(),
                                alias: alias.clone(),
                            };
                            select.insert(dependency, config);
                        }
                    }
                }
            } else {
                let dependency = Dependency {
                    package_id: dep.pkg.clone(),
                    target_name: target_name.clone(),
                    alias: alias.clone(),
                };
                select.insert(
                    dependency,
                    kind_info
                        .target
                        .as_ref()
                        .map(|platform| platform.to_string()),
                );
            }
        }
    }

    select
}

/// Packages may have targets that match aliases of dependents. This function
/// checks a target to see if it's an unexpected type for a dependency.
fn is_ignored_package_target(target: &Target) -> bool {
    target.kind.iter().any(|t| {
        matches!(
            t,
            cargo_metadata::TargetKind::Example
                | cargo_metadata::TargetKind::Bench
                | cargo_metadata::TargetKind::Test
        )
    })
}

fn is_lib_package(package: &Package) -> bool {
    package.targets.iter().any(|target| {
        target.crate_types.iter().any(|t| {
            matches!(
                t,
                cargo_metadata::CrateType::Lib | cargo_metadata::CrateType::RLib
            ) && !is_ignored_package_target(target)
        })
    })
}

fn is_proc_macro_package(package: &Package) -> bool {
    package.targets.iter().any(|target| {
        target
            .crate_types
            .iter()
            .any(|t| matches!(t, cargo_metadata::CrateType::ProcMacro))
            && !is_ignored_package_target(target)
    })
}

fn is_dev_dependency(node_dep: &NodeDep) -> bool {
    let is_normal_dep = is_normal_dependency(node_dep);
    let is_dev_dep = node_dep
        .dep_kinds
        .iter()
        .any(|k| matches!(k.kind, cargo_metadata::DependencyKind::Development));

    // In the event that a dependency is listed as both a dev and normal dependency,
    // it's only considered a dev dependency if it's __not__ a normal dependency.
    !is_normal_dep && is_dev_dep
}

fn is_build_dependency(node_dep: &NodeDep) -> bool {
    node_dep
        .dep_kinds
        .iter()
        .any(|k| matches!(k.kind, cargo_metadata::DependencyKind::Build))
}

fn is_normal_dependency(node_dep: &NodeDep) -> bool {
    node_dep
        .dep_kinds
        .iter()
        .any(|k| matches!(k.kind, cargo_metadata::DependencyKind::Normal))
}

fn is_workspace_member(node_dep: &NodeDep, metadata: &CargoMetadata) -> bool {
    metadata
        .workspace_members
        .iter()
        .any(|id| id == &node_dep.pkg)
}

fn get_library_target_name(package: &Package, potential_name: &str) -> Result<String> {
    // If the potential name is not an alias in a dependent's package, a target's name
    // should match which means we already know what the target library name is. The
    // only exception is for targets that are otherwise ignored (like benchmarks or examples).
    if package
        .targets
        .iter()
        .any(|t| t.name == potential_name && !is_ignored_package_target(t))
    {
        return Ok(potential_name.to_string());
    }

    // Locate any library type targets
    let lib_targets: Vec<&cargo_metadata::Target> = package
        .targets
        .iter()
        .filter(|t| {
            t.kind.iter().any(|k| {
                matches!(
                    k,
                    cargo_metadata::TargetKind::Lib
                        | cargo_metadata::TargetKind::RLib
                        | cargo_metadata::TargetKind::ProcMacro
                )
            })
        })
        .collect();

    // Only one target should be found
    if lib_targets.len() != 1 {
        bail!(
            "Unexpected number of 'library-like' targets found for {}: {:?}",
            package.name,
            package.targets
        )
    }

    let target = lib_targets.into_iter().last().unwrap();
    Ok(target.name.clone())
}

/// The resolve graph (resolve.nodes[#].deps[#].name) of Cargo metadata uses module names
/// for targets where packages (packages[#].targets[#].name) uses crate names. In order to
/// determine whether or not a dependency is aliased, we compare it with all available targets
/// on it's package. Note that target names are not guaranteed to be module names where Node
/// dependencies are, so we need to do a conversion to check for this. This function will
/// return the name of a target's alias in the content of the current dependent if it is aliased.
fn get_target_alias(target_name: &str, package: &Package) -> Option<String> {
    match package
        .targets
        .iter()
        .filter(|t| !is_ignored_package_target(t))
        .all(|t| sanitize_module_name(&t.name) != target_name)
    {
        true => Some(target_name.to_string()),
        false => None,
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use semver::Version;

    use crate::metadata::CargoTreeEntry;
    use crate::test::*;

    #[test]
    fn get_expected_lib_target_name() {
        let mut package = mock_cargo_metadata_package();
        package
            .targets
            .extend(vec![serde_json::from_value(serde_json::json!({
                "name": "potential",
                "kind": ["lib"],
                "crate_types": [],
                "required_features": [],
                "src_path": "/tmp/mock.rs",
                "edition": "2021",
                "doctest": false,
                "test": false,
                "doc": false,
            }))
            .unwrap()]);

        assert_eq!(
            get_library_target_name(&package, "potential").unwrap(),
            "potential"
        );
    }

    #[test]
    fn get_lib_target_name() {
        let mut package = mock_cargo_metadata_package();
        package
            .targets
            .extend(vec![serde_json::from_value(serde_json::json!({
                "name": "lib_target",
                "kind": ["lib"],
                "crate_types": [],
                "required_features": [],
                "src_path": "/tmp/mock.rs",
                "edition": "2021",
                "doctest": false,
                "test": false,
                "doc": false,
            }))
            .unwrap()]);

        assert_eq!(
            get_library_target_name(&package, "mock-pkg").unwrap(),
            "lib_target"
        );
    }

    #[test]
    fn get_rlib_target_name() {
        let mut package = mock_cargo_metadata_package();
        package
            .targets
            .extend(vec![serde_json::from_value(serde_json::json!({
                "name": "rlib_target",
                "kind": ["rlib"],
                "crate_types": [],
                "required_features": [],
                "src_path": "/tmp/mock.rs",
                "edition": "2021",
                "doctest": false,
                "test": false,
                "doc": false,
            }))
            .unwrap()]);

        assert_eq!(
            get_library_target_name(&package, "mock-pkg").unwrap(),
            "rlib_target"
        );
    }

    #[test]
    fn get_proc_macro_target_name() {
        let mut package = mock_cargo_metadata_package();
        package
            .targets
            .extend(vec![serde_json::from_value(serde_json::json!({
                "name": "proc_macro_target",
                "kind": ["proc-macro"],
                "crate_types": [],
                "required_features": [],
                "src_path": "/tmp/mock.rs",
                "edition": "2021",
                "doctest": false,
                "test": false,
                "doc": false,
            }))
            .unwrap()]);

        assert_eq!(
            get_library_target_name(&package, "mock-pkg").unwrap(),
            "proc_macro_target"
        );
    }

    #[test]
    fn get_bin_target_name() {
        let mut package = mock_cargo_metadata_package();
        package
            .targets
            .extend(vec![serde_json::from_value(serde_json::json!({
                "name": "bin_target",
                "kind": ["bin"],
                "crate_types": [],
                "required_features": [],
                "src_path": "/tmp/mock.rs",
                "edition": "2021",
                "doctest": false,
                "test": false,
                "doc": false,
            }))
            .unwrap()]);

        // It's an error for no library target to be found.
        assert!(get_library_target_name(&package, "mock-pkg").is_err());
    }

    /// Locate the [cargo_metadata::Node] for the crate matching the given name
    fn find_metadata_node<'a>(
        name: &str,
        metadata: &'a cargo_metadata::Metadata,
    ) -> &'a cargo_metadata::Node {
        metadata
            .resolve
            .as_ref()
            .unwrap()
            .nodes
            .iter()
            .find(|node| {
                let pkg = &metadata[&node.id];
                pkg.name == name
            })
            .unwrap_or_else(|| panic!("Unable to find node '{}'", name))
    }

    #[test]
    fn example_proc_macro_dep() {
        let metadata = metadata::example_proc_macro_dep();

        let node = find_metadata_node("example-proc-macro-dep", &metadata);
        let dependencies =
            DependencySet::new_for_node(node, &metadata, &TreeResolverMetadata::default());

        let normal_deps: Vec<_> = dependencies
            .normal_deps
            .items()
            .into_iter()
            .map(|(_, dep)| dep.target_name)
            .collect();
        assert_eq!(normal_deps, vec!["proc_macro_rules"]);

        let proc_macro_deps: Vec<_> = dependencies
            .proc_macro_deps
            .items()
            .into_iter()
            .map(|(_, dep)| dep.target_name)
            .collect();
        assert_eq!(proc_macro_deps, Vec::<&str>::new());
    }

    #[test]
    fn bench_name_alias_dep() {
        let metadata = metadata::alias();

        let node = find_metadata_node("surrealdb-core", &metadata);
        let dependencies =
            DependencySet::new_for_node(node, &metadata, &TreeResolverMetadata::default());

        let bindings = dependencies.normal_deps.items();

        // It's critical that the dep be found with the correct name and not the
        // alias that the `aliases` package is using that coincidentally matches the
        // `bench` target `executor` in the `async-executor` package.
        let async_executor = bindings
            .iter()
            .find(|(_, dep)| dep.target_name == "async_executor")
            .map(|(_, dep)| dep)
            .unwrap();

        // Ensure alias data is still tracked.
        assert_eq!(async_executor.alias, Some("executor".to_owned()));
    }

    #[test]
    fn sys_dependencies() {
        let metadata = metadata::build_scripts();

        let openssl_node = find_metadata_node("openssl", &metadata);

        let dependencies =
            DependencySet::new_for_node(openssl_node, &metadata, &TreeResolverMetadata::default());

        let normal_sys_crate =
            dependencies
                .normal_deps
                .items()
                .into_iter()
                .find(|(configuration, dep)| {
                    let pkg = &metadata[&dep.package_id];
                    configuration.is_none() && pkg.name == "openssl-sys"
                });

        let link_dep_sys_crate =
            dependencies
                .build_link_deps
                .items()
                .into_iter()
                .find(|(configuration, dep)| {
                    let pkg = &metadata[&dep.package_id];
                    configuration.is_none() && pkg.name == "openssl-sys"
                });

        // sys crates like `openssl-sys` should always be dependencies of any
        // crate which matches it's name minus the `-sys` suffix
        assert!(normal_sys_crate.is_some());
        assert!(link_dep_sys_crate.is_some());
    }

    #[test]
    fn sys_crate_with_build_script() {
        let metadata = metadata::build_scripts();

        let libssh2 = find_metadata_node("libssh2-sys", &metadata);
        let libssh2_depset =
            DependencySet::new_for_node(libssh2, &metadata, &TreeResolverMetadata::default());

        // Collect build dependencies into a set
        let build_deps: BTreeSet<String> = libssh2_depset
            .build_deps
            .values()
            .into_iter()
            .map(|dep| dep.package_id.repr)
            .collect();

        assert_eq!(
            BTreeSet::from([
                "registry+https://github.com/rust-lang/crates.io-index#cc@1.0.72".to_owned(),
                "registry+https://github.com/rust-lang/crates.io-index#pkg-config@0.3.24"
                    .to_owned(),
                "registry+https://github.com/rust-lang/crates.io-index#vcpkg@0.2.15".to_owned()
            ]),
            build_deps,
        );

        // Collect normal dependencies into a set
        let normal_deps: BTreeSet<String> = libssh2_depset
            .normal_deps
            .values()
            .into_iter()
            .map(|dep| dep.package_id.to_string())
            .collect();

        assert_eq!(
            BTreeSet::from([
                "registry+https://github.com/rust-lang/crates.io-index#libc@0.2.112".to_owned(),
                "registry+https://github.com/rust-lang/crates.io-index#libz-sys@1.1.8".to_owned(),
                "registry+https://github.com/rust-lang/crates.io-index#openssl-sys@0.9.87"
                    .to_owned(),
            ]),
            normal_deps,
        );

        assert!(libssh2_depset.proc_macro_deps.is_empty());
        assert!(libssh2_depset.normal_dev_deps.is_empty());
        assert!(libssh2_depset.proc_macro_dev_deps.is_empty());
        assert!(libssh2_depset.build_proc_macro_deps.is_empty());
    }

    #[test]
    fn tracked_aliases() {
        let metadata = metadata::alias();

        let aliases_node = find_metadata_node("aliases", &metadata);
        let dependencies =
            DependencySet::new_for_node(aliases_node, &metadata, &TreeResolverMetadata::default());

        let aliases: Vec<Dependency> = dependencies
            .normal_deps
            .items()
            .into_iter()
            .filter(|(configuration, dep)| configuration.is_none() && dep.alias.is_some())
            .map(|(_, dep)| dep)
            .collect();

        assert_eq!(aliases.len(), 2);

        let expected: BTreeSet<String> =
            aliases.into_iter().map(|dep| dep.alias.unwrap()).collect();

        assert_eq!(
            expected,
            BTreeSet::from(["pinned_log".to_owned(), "pinned_names".to_owned()])
        );
    }

    #[test]
    fn matched_rlib() {
        let metadata = metadata::crate_types();

        let node = find_metadata_node("crate-types", &metadata);
        let dependencies =
            DependencySet::new_for_node(node, &metadata, &TreeResolverMetadata::default());

        let rlib_deps: Vec<Dependency> = dependencies
            .normal_deps
            .items()
            .into_iter()
            .filter(|(configuration, dep)| {
                let pkg = &metadata[&dep.package_id];
                configuration.is_none()
                    && pkg.targets.iter().any(|t| {
                        t.crate_types
                            .iter()
                            .any(|t| matches!(t, cargo_metadata::CrateType::RLib))
                    })
            })
            .map(|(_, dep)| dep)
            .collect();

        // Currently the only expected __explicitly__ "rlib" target in this metadata is `sysinfo`.
        assert_eq!(rlib_deps.len(), 1);

        let sysinfo_dep = rlib_deps.iter().last().unwrap();
        assert_eq!(sysinfo_dep.target_name, "sysinfo");
    }

    #[test]
    fn multiple_dep_kinds() {
        let metadata = metadata::multi_cfg_dep();

        let node = find_metadata_node("cpufeatures", &metadata);
        let dependencies =
            DependencySet::new_for_node(node, &metadata, &TreeResolverMetadata::default());

        let libc_cfgs: BTreeSet<Option<String>> = dependencies
            .normal_deps
            .items()
            .into_iter()
            .filter(|(_, dep)| dep.target_name == "libc")
            .map(|(configuration, _)| configuration)
            .collect();

        assert_eq!(
            BTreeSet::from([
                Some("aarch64-linux-android".to_owned()),
                Some("cfg(all(target_arch = \"aarch64\", target_os = \"linux\"))".to_owned()),
                Some("cfg(all(target_arch = \"aarch64\", target_vendor = \"apple\"))".to_owned()),
            ]),
            libc_cfgs,
        );
    }

    #[test]
    fn multi_kind_proc_macro_dep() {
        let metadata = metadata::multi_kind_proc_macro_dep();

        let node = find_metadata_node("multi-kind-proc-macro-dep", &metadata);
        let dependencies =
            DependencySet::new_for_node(node, &metadata, &TreeResolverMetadata::default());

        let lib_deps: Vec<_> = dependencies
            .proc_macro_deps
            .items()
            .into_iter()
            .map(|(_, dep)| dep.target_name)
            .collect();
        assert_eq!(lib_deps, vec!["paste"]);

        let build_deps: Vec<_> = dependencies
            .build_proc_macro_deps
            .items()
            .into_iter()
            .map(|(_, dep)| dep.target_name)
            .collect();
        assert_eq!(build_deps, vec!["paste"]);
    }

    #[test]
    fn optional_deps_disabled() {
        let metadata = metadata::optional_deps_disabled();

        let node = find_metadata_node("clap", &metadata);
        let dependencies =
            DependencySet::new_for_node(node, &metadata, &TreeResolverMetadata::default());

        assert!(!dependencies
            .normal_deps
            .items()
            .iter()
            .any(|(configuration, dep)| configuration.is_none()
                && (dep.target_name == "is-terminal" || dep.target_name == "termcolor")));
    }

    #[test]
    fn renamed_optional_deps_disabled() {
        let metadata = metadata::renamed_optional_deps_disabled();

        let serde_with = find_metadata_node("serde_with", &metadata);
        let serde_with_depset =
            DependencySet::new_for_node(serde_with, &metadata, &TreeResolverMetadata::new());
        assert!(!serde_with_depset
            .normal_deps
            .items()
            .iter()
            .any(|(configuration, dep)| configuration.is_none() && dep.target_name == "indexmap"));
    }

    #[test]
    fn optional_deps_enabled() {
        let metadata = metadata::optional_deps_enabled();
        let mut select = Select::new();
        select.insert(
            CargoTreeEntry {
                features: BTreeSet::new(),
                deps: BTreeSet::from([
                    CrateId::new("is-terminal".to_owned(), Version::new(0, 4, 5)),
                    CrateId::new("termcolor".to_owned(), Version::new(1, 2, 0)),
                ]),
            },
            None,
        );
        let resolver_data = TreeResolverMetadata::from([(
            CrateId::new("clap".to_owned(), Version::new(4, 1, 1)),
            select,
        )]);

        let clap = find_metadata_node("clap", &metadata);
        let clap_depset = DependencySet::new_for_node(clap, &metadata, &resolver_data);
        assert_eq!(
            clap_depset
                .normal_deps
                .items()
                .iter()
                .filter(|(configuration, dep)| configuration.is_none()
                    && (dep.target_name == "is_terminal" || dep.target_name == "termcolor"))
                .count(),
            2
        );

        let notify = find_metadata_node("notify", &metadata);
        let notify_depset =
            DependencySet::new_for_node(notify, &metadata, &TreeResolverMetadata::default());

        // mio is not present in the common list of dependencies
        assert!(!notify_depset
            .normal_deps
            .items()
            .iter()
            .any(|(configuration, dep)| configuration.is_none() && dep.target_name == "mio"));

        // mio is a dependency on linux
        assert!(notify_depset
            .normal_deps
            .items()
            .iter()
            .any(|(configuration, dep)| configuration.as_deref()
                == Some("cfg(target_os = \"linux\")")
                && dep.target_name == "mio"));

        // mio is marked optional=true on macos
        assert!(!notify_depset
            .normal_deps
            .items()
            .iter()
            .any(|(configuration, dep)| configuration.as_deref()
                == Some("cfg(target_os = \"macos\")")
                && dep.target_name == "mio"));
    }

    #[test]
    fn optional_deps_disabled_build_dep_enabled() {
        let metadata = metadata::optional_deps_disabled_build_dep_enabled();

        let node = find_metadata_node("gherkin", &metadata);
        let dependencies =
            DependencySet::new_for_node(node, &metadata, &TreeResolverMetadata::default());

        assert!(!dependencies
            .normal_deps
            .items()
            .iter()
            .any(|(configuration, dep)| configuration.is_none() && dep.target_name == "serde"));

        assert!(dependencies
            .build_deps
            .items()
            .iter()
            .any(|(configuration, dep)| configuration.is_none() && dep.target_name == "serde"));
    }

    #[test]
    fn renamed_optional_deps_enabled() {
        let metadata = metadata::renamed_optional_deps_enabled();

        let mut select = Select::new();
        select.insert(
            CargoTreeEntry {
                features: BTreeSet::new(),
                deps: BTreeSet::from([CrateId::new("ecdsa".to_owned(), Version::new(0, 16, 8))]),
            },
            None,
        );
        let resolver_data = TreeResolverMetadata::from([(
            CrateId::new("p256".to_owned(), Version::new(0, 13, 2)),
            select,
        )]);

        let p256 = find_metadata_node("p256", &metadata);
        let p256_depset = DependencySet::new_for_node(p256, &metadata, &resolver_data);
        assert_eq!(
            p256_depset
                .normal_deps
                .items()
                .iter()
                .filter(|(configuration, dep)| configuration.is_none() && dep.target_name == "ecdsa")
                .count(),
            1
        );
    }

    #[test]
    fn tree_resolver_deps() {
        let metadata = metadata::resolver_2_deps();

        let mut select = Select::new();
        select.insert(
            CargoTreeEntry {
                deps: BTreeSet::from([
                    CrateId::new("libc".to_owned(), Version::new(0, 2, 153)),
                    CrateId::new("mio".to_owned(), Version::new(0, 8, 11)),
                    CrateId::new("socket2".to_owned(), Version::new(0, 5, 6)),
                ]),
                features: BTreeSet::from([
                    "io-std".to_owned(),
                    "libc".to_owned(),
                    "mio".to_owned(),
                    "net".to_owned(),
                    "rt".to_owned(),
                    "socket2".to_owned(),
                    "sync".to_owned(),
                    "time".to_owned(),
                ]),
            },
            Some("x86_64-unknown-linux-gnu".to_owned()),
        );
        select.insert(
            CargoTreeEntry {
                deps: BTreeSet::from([
                    CrateId::new("bytes".to_owned(), Version::new(1, 6, 0)),
                    CrateId::new("pin-project-lite".to_owned(), Version::new(0, 2, 14)),
                ]),
                features: BTreeSet::from([
                    "bytes".to_owned(),
                    "default".to_owned(),
                    "io-util".to_owned(),
                ]),
            },
            None,
        );

        let tree_metadata = TreeResolverMetadata::from([(
            CrateId::new("tokio".to_owned(), Version::new(1, 37, 0)),
            select,
        )]);

        let tokio_node = find_metadata_node("tokio", &metadata);
        let tokio_depset = DependencySet::new_for_node(tokio_node, &metadata, &tree_metadata);
        assert_eq!(
            tokio_depset
                .normal_deps
                .items()
                .iter()
                .filter(|(configuration, dep)| {
                    let is_common = configuration.is_none();
                    let is_mio =
                        dep.target_name == "mio" || dep.package_id.to_string().contains("mio");

                    is_common && is_mio
                })
                .count(),
            0,
            "`mio` is a platform specific dependency and therefore should not be identified under the common configuration."
        );
    }
}
