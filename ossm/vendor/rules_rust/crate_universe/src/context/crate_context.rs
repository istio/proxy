//! Crate specific information embedded into [crate::context::Context] objects.

use std::collections::{BTreeMap, BTreeSet};

use cargo_metadata::{Node, Package, PackageId};
use serde::{Deserialize, Serialize};

use crate::config::{AliasRule, CrateId, GenBinaries};
use crate::metadata::{
    CrateAnnotation, Dependency, PairedExtras, SourceAnnotation, TreeResolverMetadata,
};
use crate::select::Select;
use crate::utils::sanitize_module_name;
use crate::utils::starlark::{Glob, Label};

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub struct CrateDependency {
    /// The [CrateId] of the dependency
    pub id: CrateId,

    /// The target name of the dependency. Note this may differ from the
    /// dependency's package name in cases such as build scripts.
    pub target: String,

    /// Some dependencies are assigned aliases. This is tracked here
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub alias: Option<String>,
}

#[derive(Debug, Default, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Clone)]
#[serde(default)]
pub(crate) struct TargetAttributes {
    /// The module name of the crate (notably, not the package name).
    //
    // This must be the first field of `TargetAttributes` to make it the
    // lexicographically first thing the derived `Ord` implementation will sort
    // by. The `Ord` impl controls the order of multiple rules of the same type
    // in the same BUILD file. In particular, this makes packages with multiple
    // bin crates generate those `rust_binary` targets in alphanumeric order.
    pub(crate) crate_name: String,

    /// The path to the crate's root source file, relative to the manifest.
    pub(crate) crate_root: Option<String>,

    /// A glob pattern of all source files required by the target
    pub(crate) srcs: Glob,
}

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Clone)]
pub(crate) enum Rule {
    /// `rust_library`
    Library(TargetAttributes),

    /// `rust_proc_macro`
    ProcMacro(TargetAttributes),

    /// `rust_binary`
    Binary(TargetAttributes),

    /// `cargo_build_script`
    BuildScript(TargetAttributes),
}

impl Rule {
    /// The keys that can be used in override_targets to override these Rule sources.
    /// These intentionally match the accepted `Target.kind`s returned by cargo-metadata.
    pub(crate) fn override_target_key(&self) -> &'static str {
        match self {
            Self::Library(..) => "lib",
            Self::ProcMacro(..) => "proc-macro",
            Self::Binary(..) => "bin",
            Self::BuildScript(..) => "custom-build",
        }
    }

    pub(crate) fn crate_name(&self) -> &str {
        match self {
            Self::Library(attrs)
            | Self::ProcMacro(attrs)
            | Self::Binary(attrs)
            | Self::BuildScript(attrs) => &attrs.crate_name,
        }
    }
}

/// A set of attributes common to most `rust_library`, `rust_proc_macro`, and other
/// [core rules of `rules_rust`](https://bazelbuild.github.io/rules_rust/defs.html).
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default)]
pub(crate) struct CommonAttributes {
    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) compile_data: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "BTreeSet::is_empty")]
    pub(crate) compile_data_glob: BTreeSet<String>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) crate_features: Select<BTreeSet<String>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) data: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "BTreeSet::is_empty")]
    pub(crate) data_glob: BTreeSet<String>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) deps: Select<BTreeSet<CrateDependency>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) extra_deps: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) deps_dev: Select<BTreeSet<CrateDependency>>,

    pub(crate) edition: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) linker_script: Option<String>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) proc_macro_deps: Select<BTreeSet<CrateDependency>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) extra_proc_macro_deps: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) proc_macro_deps_dev: Select<BTreeSet<CrateDependency>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) rustc_env: Select<BTreeMap<String, String>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) rustc_env_files: Select<BTreeSet<String>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) rustc_flags: Select<Vec<String>>,

    pub(crate) version: String,

    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub(crate) tags: Vec<String>,
}

impl Default for CommonAttributes {
    fn default() -> Self {
        Self {
            compile_data: Default::default(),
            // Generated targets include all files in their package by default
            compile_data_glob: BTreeSet::from(["**".to_owned()]),
            crate_features: Default::default(),
            data: Default::default(),
            data_glob: Default::default(),
            deps: Default::default(),
            extra_deps: Default::default(),
            deps_dev: Default::default(),
            edition: Default::default(),
            linker_script: Default::default(),
            proc_macro_deps: Default::default(),
            extra_proc_macro_deps: Default::default(),
            proc_macro_deps_dev: Default::default(),
            rustc_env: Default::default(),
            rustc_env_files: Default::default(),
            rustc_flags: Default::default(),
            version: Default::default(),
            tags: Default::default(),
        }
    }
}

// Build script attributes. See
// https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default)]
pub(crate) struct BuildScriptAttributes {
    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) compile_data: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "BTreeSet::is_empty")]
    pub(crate) compile_data_glob: BTreeSet<String>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) data: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "BTreeSet::is_empty")]
    pub(crate) data_glob: BTreeSet<String>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) deps: Select<BTreeSet<CrateDependency>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) extra_deps: Select<BTreeSet<Label>>,

    // TODO: refactor a crate with a build.rs file from two into three bazel
    // rules in order to deduplicate link_dep information. Currently as the
    // crate depends upon the build.rs file, the build.rs cannot find the
    // information for the normal dependencies of the crate. This could be
    // solved by switching the dependency graph from:
    //
    //   rust_library -> cargo_build_script
    //
    // to:
    //
    //   rust_library ->-+-->------------------->--+
    //                   |                         |
    //                   +--> cargo_build_script --+--> crate dependencies
    //
    // in which either all of the deps are in crate dependencies, or just the
    // normal dependencies. This could be handled a special rule, or just using
    // a `filegroup`.
    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) link_deps: Select<BTreeSet<CrateDependency>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) extra_link_deps: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) build_script_env: Select<BTreeMap<String, String>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) rundir: Select<String>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) extra_proc_macro_deps: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) proc_macro_deps: Select<BTreeSet<CrateDependency>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) rustc_env: Select<BTreeMap<String, String>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) rustc_flags: Select<Vec<String>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) rustc_env_files: Select<BTreeSet<String>>,

    #[serde(skip_serializing_if = "Select::is_empty")]
    pub(crate) tools: Select<BTreeSet<Label>>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) links: Option<String>,

    #[serde(skip_serializing_if = "BTreeSet::is_empty")]
    pub(crate) toolchains: BTreeSet<Label>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) use_default_shell_env: Option<i32>,
}

impl Default for BuildScriptAttributes {
    fn default() -> Self {
        Self {
            compile_data: Default::default(),
            // The build script itself also has access to all
            // source files by default.
            compile_data_glob: BTreeSet::from(["**".to_owned()]),
            data: Default::default(),
            // Build scripts include all sources by default
            data_glob: BTreeSet::from(["**".to_owned()]),
            deps: Default::default(),
            extra_deps: Default::default(),
            link_deps: Default::default(),
            extra_link_deps: Default::default(),
            build_script_env: Default::default(),
            rundir: Default::default(),
            extra_proc_macro_deps: Default::default(),
            proc_macro_deps: Default::default(),
            rustc_env: Default::default(),
            rustc_flags: Default::default(),
            rustc_env_files: Default::default(),
            tools: Default::default(),
            links: Default::default(),
            toolchains: Default::default(),
            use_default_shell_env: None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub(crate) struct CrateContext {
    /// The package name of the current crate
    pub(crate) name: String,

    /// The full version of the current crate
    pub(crate) version: semver::Version,

    /// The package URL of the current crate
    #[serde(default)]
    pub(crate) package_url: Option<String>,

    /// Optional source annotations if they were discoverable in the
    /// lockfile. Workspace Members will not have source annotations and
    /// potentially others.
    #[serde(default)]
    pub(crate) repository: Option<SourceAnnotation>,

    /// A list of all targets (lib, proc-macro, bin) associated with this package
    #[serde(default)]
    pub(crate) targets: BTreeSet<Rule>,

    /// The name of the crate's root library target. This is the target that a dependent
    /// would get if they were to depend on `{crate_name}`.
    #[serde(default)]
    pub(crate) library_target_name: Option<String>,

    /// A set of attributes common to most [Rule] types or target types.
    #[serde(default)]
    pub(crate) common_attrs: CommonAttributes,

    /// Optional attributes for build scripts. This field is only populated if
    /// a build script (`custom-build`) target is defined for the crate.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub(crate) build_script_attrs: Option<BuildScriptAttributes>,

    /// The license used by the crate
    #[serde(default)]
    pub(crate) license: Option<String>,

    /// The SPDX licence IDs
    /// #[serde(default)]
    pub(crate) license_ids: BTreeSet<String>,

    /// The license file
    #[serde(default)]
    pub(crate) license_file: Option<String>,

    /// Additional text to add to the generated BUILD file.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub(crate) additive_build_file_content: Option<String>,

    /// If true, disables pipelining for library targets generated for this crate
    #[serde(skip_serializing_if = "std::ops::Not::not")]
    #[serde(default)]
    pub(crate) disable_pipelining: bool,

    /// Extra targets that should be aliased.
    #[serde(skip_serializing_if = "BTreeMap::is_empty")]
    #[serde(default)]
    pub(crate) extra_aliased_targets: BTreeMap<String, String>,

    /// Transition rule to use instead of `alias`.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    pub(crate) alias_rule: Option<AliasRule>,

    /// Targets to use instead of the default target for the crate.
    #[serde(skip_serializing_if = "BTreeMap::is_empty")]
    #[serde(default)]
    pub(crate) override_targets: BTreeMap<String, Label>,
}

impl CrateContext {
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn new(
        annotation: &CrateAnnotation,
        packages: &BTreeMap<PackageId, Package>,
        source_annotations: &BTreeMap<PackageId, SourceAnnotation>,
        extras: &BTreeMap<CrateId, PairedExtras>,
        resolver_data: &TreeResolverMetadata,
        include_binaries: bool,
        include_build_scripts: bool,
        sources_are_present: bool,
    ) -> anyhow::Result<Self> {
        let package: &Package = &packages[&annotation.node.id];
        let current_crate_id = CrateId::new(package.name.clone(), package.version.clone());

        let new_crate_dep = |dep: Dependency| -> CrateDependency {
            let pkg = &packages[&dep.package_id];

            // Unfortunately, The package graph and resolve graph of cargo metadata have different representations
            // for the crate names (resolve graph sanitizes names to match module names) so to get the rest of this
            // content to align when rendering, the dependency target needs to be explicitly sanitized.
            let target = sanitize_module_name(&dep.target_name);

            CrateDependency {
                id: CrateId::new(pkg.name.clone(), pkg.version.clone()),
                target,
                alias: dep.alias,
            }
        };

        // Convert the dependencies into renderable strings
        let deps = annotation.deps.normal_deps.clone().map(new_crate_dep);
        let deps_dev = annotation.deps.normal_dev_deps.clone().map(new_crate_dep);
        let proc_macro_deps = annotation.deps.proc_macro_deps.clone().map(new_crate_dep);
        let proc_macro_deps_dev = annotation
            .deps
            .proc_macro_dev_deps
            .clone()
            .map(new_crate_dep);

        let crate_features = resolver_data
            .get(&current_crate_id)
            .map(|tree_data| {
                let mut select = Select::<BTreeSet<String>>::new();
                for (config, data) in tree_data.items() {
                    for feature in data.features {
                        select.insert(feature, config.clone());
                    }
                }
                select
            })
            .unwrap_or_default();

        // Gather all "common" attributes
        let mut common_attrs = CommonAttributes {
            crate_features,
            deps,
            deps_dev,
            edition: package.edition.as_str().to_string(),
            proc_macro_deps,
            proc_macro_deps_dev,
            version: package.version.to_string(),
            ..Default::default()
        };

        // Locate extra settings for the current package.
        let package_extra = extras
            .iter()
            .find(|(_, settings)| settings.package_id == package.id);

        let include_build_scripts =
            Self::crate_includes_build_script(package_extra, include_build_scripts);

        let gen_none = GenBinaries::Some(BTreeSet::new());
        let gen_binaries = package_extra
            .and_then(|(_, settings)| settings.crate_extra.gen_binaries.as_ref())
            .unwrap_or(if include_binaries {
                &GenBinaries::All
            } else {
                &gen_none
            });

        // Iterate over each target and produce a Bazel target for all supported "kinds"
        let targets = Self::collect_targets(
            &annotation.node,
            packages,
            gen_binaries,
            include_build_scripts,
            sources_are_present,
        )?;

        // Parse the library crate name from the set of included targets
        let library_target_name = {
            let lib_targets: Vec<&TargetAttributes> = targets
                .iter()
                .filter_map(|t| match t {
                    Rule::ProcMacro(attrs) => Some(attrs),
                    Rule::Library(attrs) => Some(attrs),
                    _ => None,
                })
                .collect();

            // TODO: There should only be at most 1 library target. This case
            // should be handled in a more intelligent way.
            assert!(lib_targets.len() <= 1);
            lib_targets
                .iter()
                .last()
                .map(|attr| attr.crate_name.clone())
        };

        // Gather any build-script related attributes
        let build_script_target = targets.iter().find_map(|r| match r {
            Rule::BuildScript(attr) => Some(attr),
            _ => None,
        });

        let build_script_attrs = if let Some(target) = build_script_target {
            // Track the build script dependency
            common_attrs.deps.insert(
                CrateDependency {
                    id: current_crate_id,
                    target: target.crate_name.clone(),
                    alias: None,
                },
                None,
            );

            let build_deps = annotation.deps.build_deps.clone().map(new_crate_dep);
            let build_link_deps = annotation.deps.build_link_deps.clone().map(new_crate_dep);
            let build_proc_macro_deps = annotation
                .deps
                .build_proc_macro_deps
                .clone()
                .map(new_crate_dep);

            Some(BuildScriptAttributes {
                deps: build_deps,
                link_deps: build_link_deps,
                proc_macro_deps: build_proc_macro_deps,
                links: package.links.clone(),
                ..Default::default()
            })
        } else {
            None
        };

        // Save the repository information for the current crate
        let repository = source_annotations.get(&package.id).cloned();

        // Identify the license type
        let mut license_ids: BTreeSet<String> = BTreeSet::new();
        if let Some(license) = &package.license {
            if let Ok(parse_result) = spdx::Expression::parse_mode(license, spdx::ParseMode::LAX) {
                parse_result.requirements().for_each(|er| {
                    if let Some(license_id) = er.req.license.id() {
                        license_ids.insert(license_id.name.to_string());
                    }
                });
            }
        }

        let license_file = Self::locate_license_file(package);

        let package_url: Option<String> = match package.repository {
            Some(..) => package.repository.clone(),
            None => package.homepage.clone(),
        };

        // Create the crate's context and apply extra settings
        Ok(CrateContext {
            name: package.name.clone(),
            version: package.version.clone(),
            license: package.license.clone(),
            license_ids,
            license_file,
            package_url,
            repository,
            targets,
            library_target_name,
            common_attrs,
            build_script_attrs,
            additive_build_file_content: None,
            disable_pipelining: false,
            extra_aliased_targets: BTreeMap::new(),
            alias_rule: None,
            override_targets: BTreeMap::new(),
        }
        .with_overrides(extras))
    }

    fn with_overrides(mut self, extras: &BTreeMap<CrateId, PairedExtras>) -> Self {
        let id = CrateId::new(self.name.clone(), self.version.clone());

        // Insert all overrides/extras
        if let Some(paired_override) = extras.get(&id) {
            let crate_extra = &paired_override.crate_extra;

            // Deps
            if let Some(extra) = &crate_extra.deps {
                self.common_attrs.extra_deps =
                    Select::merge(self.common_attrs.extra_deps, extra.clone());
            }

            // Proc macro deps
            if let Some(extra) = &crate_extra.proc_macro_deps {
                self.common_attrs.extra_proc_macro_deps =
                    Select::merge(self.common_attrs.extra_proc_macro_deps, extra.clone());
            }

            // Compile data
            if let Some(extra) = &crate_extra.compile_data {
                self.common_attrs.compile_data =
                    Select::merge(self.common_attrs.compile_data, extra.clone());
            }

            // Compile data glob
            if let Some(extra) = &crate_extra.compile_data_glob {
                self.common_attrs.compile_data_glob.extend(extra.clone());
            }

            // Crate features
            if let Some(extra) = &crate_extra.crate_features {
                self.common_attrs.crate_features =
                    Select::merge(self.common_attrs.crate_features, extra.clone());
            }

            // Data
            if let Some(extra) = &crate_extra.data {
                self.common_attrs.data = Select::merge(self.common_attrs.data, extra.clone());
            }

            // Data glob
            if let Some(extra) = &crate_extra.data_glob {
                self.common_attrs.data_glob.extend(extra.clone());
            }

            // Disable pipelining
            if crate_extra.disable_pipelining {
                self.disable_pipelining = true;
            }

            // Rustc flags
            if let Some(extra) = &crate_extra.rustc_flags {
                self.common_attrs.rustc_flags =
                    Select::merge(self.common_attrs.rustc_flags, extra.clone());
            }

            // Rustc env
            if let Some(extra) = &crate_extra.rustc_env {
                self.common_attrs.rustc_env =
                    Select::merge(self.common_attrs.rustc_env, extra.clone());
            }

            // Rustc env files
            if let Some(extra) = &crate_extra.rustc_env_files {
                self.common_attrs.rustc_env_files =
                    Select::merge(self.common_attrs.rustc_env_files, extra.clone());
            }

            // Build script Attributes
            if let Some(attrs) = &mut self.build_script_attrs {
                // Deps
                if let Some(extra) = &crate_extra.build_script_deps {
                    attrs.extra_deps = Select::merge(attrs.extra_deps.clone(), extra.clone());
                }

                // Proc macro deps
                if let Some(extra) = &crate_extra.build_script_proc_macro_deps {
                    attrs.extra_proc_macro_deps =
                        Select::merge(attrs.extra_proc_macro_deps.clone(), extra.clone());
                }

                // Data
                if let Some(extra) = &crate_extra.build_script_data {
                    attrs.data = Select::merge(attrs.data.clone(), extra.clone());
                }

                // Compile Data
                if let Some(extra) = &crate_extra.build_script_compile_data {
                    attrs.compile_data = Select::merge(attrs.compile_data.clone(), extra.clone());
                }

                // Tools
                if let Some(extra) = &crate_extra.build_script_tools {
                    attrs.tools = Select::merge(attrs.tools.clone(), extra.clone());
                }

                // Toolchains
                if let Some(extra) = &crate_extra.build_script_toolchains {
                    attrs.toolchains.extend(extra.iter().cloned());
                }

                // Data glob
                if let Some(extra) = &crate_extra.build_script_data_glob {
                    attrs.data_glob.extend(extra.clone());
                }

                // Rustc env
                if let Some(extra) = &crate_extra.build_script_rustc_env {
                    attrs.rustc_env = Select::merge(attrs.rustc_env.clone(), extra.clone());
                }

                // Build script env
                if let Some(extra) = &crate_extra.build_script_env {
                    attrs.build_script_env =
                        Select::merge(attrs.build_script_env.clone(), extra.clone());
                }

                // Default Shell Env
                if let Some(extra) = &crate_extra.build_script_use_default_shell_env {
                    attrs.use_default_shell_env = Some(*extra);
                }

                if let Some(rundir) = &crate_extra.build_script_rundir {
                    attrs.rundir = Select::merge(attrs.rundir.clone(), rundir.clone());
                }
            }

            // Extra build contents
            self.additive_build_file_content = crate_extra
                .additive_build_file_content
                .as_ref()
                .map(|content| {
                    // For prettier rendering, dedent the build contents
                    textwrap::dedent(content)
                });

            // Extra aliased targets
            if let Some(extra) = &crate_extra.extra_aliased_targets {
                self.extra_aliased_targets.append(&mut extra.clone());
            }

            // Transition alias
            if let Some(alias_rule) = &crate_extra.alias_rule {
                self.alias_rule.get_or_insert(alias_rule.clone());
            }

            // Git shallow_since
            if let Some(SourceAnnotation::Git { shallow_since, .. }) = &mut self.repository {
                shallow_since.clone_from(&crate_extra.shallow_since);
            }

            // Patch attributes
            if let Some(repository) = &mut self.repository {
                match repository {
                    SourceAnnotation::Git {
                        patch_args,
                        patch_tool,
                        patches,
                        ..
                    } => {
                        patch_args.clone_from(&crate_extra.patch_args);
                        patch_tool.clone_from(&crate_extra.patch_tool);
                        patches.clone_from(&crate_extra.patches);
                    }
                    SourceAnnotation::Http {
                        patch_args,
                        patch_tool,
                        patches,
                        ..
                    } => {
                        patch_args.clone_from(&crate_extra.patch_args);
                        patch_tool.clone_from(&crate_extra.patch_tool);
                        patches.clone_from(&crate_extra.patches);
                    }
                    SourceAnnotation::Path { .. } => {
                        // We don't support applying patches to local path deps.
                    }
                }
            }

            if let Some(override_targets) = &crate_extra.override_targets {
                self.override_targets.extend(override_targets.clone());
            }
        }

        self
    }

    fn locate_license_file(package: &Package) -> Option<String> {
        if let Some(license_file_path) = &package.license_file {
            return Some(license_file_path.to_string());
        }
        let package_root = package
            .manifest_path
            .as_std_path()
            .parent()
            .expect("Every manifest should have a parent directory");
        if package_root.exists() {
            let mut paths: Vec<_> = package_root
                .read_dir()
                .unwrap()
                .map(|r| r.unwrap())
                .collect();
            paths.sort_by_key(|dir| dir.path());
            for path in paths {
                if let Some(file_name) = path.file_name().to_str() {
                    if file_name.to_uppercase().starts_with("LICENSE") {
                        return Some(file_name.to_string());
                    }
                }
            }
        }
        None
    }

    /// Determine whether or not a crate __should__ include a build script
    /// (build.rs) if it happens to have one.
    fn crate_includes_build_script(
        package_extra: Option<(&CrateId, &PairedExtras)>,
        default_generate_build_script: bool,
    ) -> bool {
        // If the crate has extra settings, which explicitly set `gen_build_script`, always use
        // this value, otherwise, fallback to the provided default.
        package_extra
            .and_then(|(_, settings)| settings.crate_extra.gen_build_script)
            .unwrap_or(default_generate_build_script)
    }

    /// Collect all Bazel targets that should be generated for a particular Package
    fn collect_targets(
        node: &Node,
        packages: &BTreeMap<PackageId, Package>,
        gen_binaries: &GenBinaries,
        include_build_scripts: bool,
        sources_are_present: bool,
    ) -> anyhow::Result<BTreeSet<Rule>> {
        let package = &packages[&node.id];

        let package_root = package
            .manifest_path
            .as_std_path()
            .parent()
            .expect("Every manifest should have a parent directory");

        package
            .targets
            .iter()
            .flat_map(|target| {
                target.kind.iter().filter_map(move |kind| {
                    // Unfortunately, The package graph and resolve graph of cargo metadata have different representations
                    // for the crate names (resolve graph sanitizes names to match module names) so to get the rest of this
                    // content to align when rendering, the package target names are always sanitized.
                    let crate_name = sanitize_module_name(&target.name);

                    if !target.src_path.starts_with(package_root) {
                        return Some(Err(anyhow::anyhow!("Package {:?} target {:?} had an absolute source path {:?}, which is not supported", package.name, target.name, target.src_path)));
                    }

                    // Locate the crate's root source file relative to the package root normalized for unix
                    let crate_root = pathdiff::diff_paths(&target.src_path, package_root).map(
                        // Normalize the path so that it always renders the same regardless of platform
                        |root| root.to_string_lossy().replace('\\', "/"),
                    );

                    // Conditionally check to see if the dependencies is a build-script target
                    if include_build_scripts && matches!(kind, cargo_metadata::TargetKind::CustomBuild) {
                        return Some(Ok(Rule::BuildScript(TargetAttributes {
                            crate_name,
                            crate_root,
                            srcs: Glob::new_rust_srcs(!sources_are_present),
                        })));
                    }

                    // Check to see if the dependencies is a proc-macro target
                    if matches!(kind, cargo_metadata::TargetKind::ProcMacro) {
                        return Some(Ok(Rule::ProcMacro(TargetAttributes {
                            crate_name,
                            crate_root,
                            srcs: Glob::new_rust_srcs(!sources_are_present),
                        })));
                    }

                    // Check to see if the dependencies is a library target
                    if matches!(kind, cargo_metadata::TargetKind::Lib | cargo_metadata::TargetKind::RLib) {
                        return Some(Ok(Rule::Library(TargetAttributes {
                            crate_name,
                            crate_root,
                            srcs: Glob::new_rust_srcs(!sources_are_present),
                        })));
                    }

                    // Check if the target kind is binary and is one of the ones included in gen_binaries
                    if matches!(kind, cargo_metadata::TargetKind::Bin)
                        && match gen_binaries {
                            GenBinaries::All => true,
                            GenBinaries::Some(set) => set.contains(&target.name),
                        }
                    {
                        return Some(Ok(Rule::Binary(TargetAttributes {
                            crate_name: target.name.clone(),
                            crate_root,
                            srcs: Glob::new_rust_srcs(!sources_are_present),
                        })));
                    }

                    None
                })
            })
            .collect()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use camino::Utf8Path;
    use semver::Version;

    use crate::config::CrateAnnotations;
    use crate::metadata::{Annotations, CargoTreeEntry};

    fn common_annotations() -> Annotations {
        Annotations::new(
            crate::test::metadata::common(),
            crate::test::lockfile::common(),
            crate::config::Config::default(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap()
    }

    #[test]
    fn new_context() {
        let annotations = common_annotations();

        let crate_annotation = &annotations.metadata.crates[&PackageId {
            repr: "path+file://{TEMP_DIR}/common#0.1.0".to_owned(),
        }];

        let include_binaries = false;
        let include_build_scripts = false;
        let are_sources_present = false;
        let context = CrateContext::new(
            crate_annotation,
            &annotations.metadata.packages,
            &annotations.lockfile.crates,
            &annotations.pairred_extras,
            &annotations.metadata.workspace_metadata.tree_metadata,
            include_binaries,
            include_build_scripts,
            are_sources_present,
        )
        .unwrap();

        assert_eq!(context.name, "common");
        assert_eq!(
            context.targets,
            BTreeSet::from([Rule::Library(TargetAttributes {
                crate_name: "common".to_owned(),
                crate_root: Some("lib.rs".to_owned()),
                srcs: Glob::new_rust_srcs(!are_sources_present),
            })]),
        );
    }

    #[test]
    fn context_with_overrides() {
        let annotations = common_annotations();

        let package_id = PackageId {
            repr: "path+file://{TEMP_DIR}/common#0.1.0".to_owned(),
        };

        let crate_annotation = &annotations.metadata.crates[&package_id];

        let mut pairred_extras = BTreeMap::new();
        pairred_extras.insert(
            CrateId::new("common".to_owned(), semver::Version::new(0, 1, 0)),
            PairedExtras {
                package_id,
                crate_extra: CrateAnnotations {
                    gen_binaries: Some(GenBinaries::All),
                    data_glob: Some(BTreeSet::from(["**/data_glob/**".to_owned()])),
                    ..CrateAnnotations::default()
                },
            },
        );

        let include_binaries = false;
        let include_build_scripts = false;
        let are_sources_present = false;
        let context = CrateContext::new(
            crate_annotation,
            &annotations.metadata.packages,
            &annotations.lockfile.crates,
            &pairred_extras,
            &annotations.metadata.workspace_metadata.tree_metadata,
            include_binaries,
            include_build_scripts,
            are_sources_present,
        )
        .unwrap();

        assert_eq!(context.name, "common");
        assert_eq!(
            context.targets,
            BTreeSet::from([
                Rule::Library(TargetAttributes {
                    crate_name: "common".to_owned(),
                    crate_root: Some("lib.rs".to_owned()),
                    srcs: Glob::new_rust_srcs(!are_sources_present),
                }),
                Rule::Binary(TargetAttributes {
                    crate_name: "common-bin".to_owned(),
                    crate_root: Some("main.rs".to_owned()),
                    srcs: Glob::new_rust_srcs(!are_sources_present),
                }),
            ]),
        );
        assert_eq!(
            context.common_attrs.data_glob,
            BTreeSet::from(["**/data_glob/**".to_owned()])
        );
    }

    fn build_script_annotations() -> Annotations {
        Annotations::new(
            crate::test::metadata::build_scripts(),
            crate::test::lockfile::build_scripts(),
            crate::config::Config::default(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap()
    }

    fn crate_type_annotations() -> Annotations {
        Annotations::new(
            crate::test::metadata::crate_types(),
            crate::test::lockfile::crate_types(),
            crate::config::Config::default(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap()
    }

    #[test]
    fn context_with_build_script() {
        let annotations = build_script_annotations();

        let package_id = PackageId {
            repr: "registry+https://github.com/rust-lang/crates.io-index#openssl-sys@0.9.87"
                .to_owned(),
        };

        let crate_annotation = &annotations.metadata.crates[&package_id];

        let include_binaries = false;
        let include_build_scripts = true;
        let are_sources_present = false;
        let context = CrateContext::new(
            crate_annotation,
            &annotations.metadata.packages,
            &annotations.lockfile.crates,
            &annotations.pairred_extras,
            &annotations.metadata.workspace_metadata.tree_metadata,
            include_binaries,
            include_build_scripts,
            are_sources_present,
        )
        .unwrap();

        assert_eq!(context.name, "openssl-sys");
        assert!(context.build_script_attrs.is_some());
        assert_eq!(
            context.targets,
            BTreeSet::from([
                Rule::Library(TargetAttributes {
                    crate_name: "openssl_sys".to_owned(),
                    crate_root: Some("src/lib.rs".to_owned()),
                    srcs: Glob::new_rust_srcs(!are_sources_present),
                }),
                Rule::BuildScript(TargetAttributes {
                    crate_name: "build_script_main".to_owned(),
                    crate_root: Some("build/main.rs".to_owned()),
                    srcs: Glob::new_rust_srcs(!are_sources_present),
                })
            ]),
        );

        // Cargo build scripts should include all sources
        assert!(context.build_script_attrs.unwrap().data_glob.contains("**"));
    }

    #[test]
    fn context_disabled_build_script() {
        let annotations = build_script_annotations();

        let package_id = PackageId {
            repr: "registry+https://github.com/rust-lang/crates.io-index#openssl-sys@0.9.87"
                .to_owned(),
        };

        let crate_annotation = &annotations.metadata.crates[&package_id];

        let include_binaries = false;
        let include_build_scripts = false;
        let are_sources_present = false;
        let context = CrateContext::new(
            crate_annotation,
            &annotations.metadata.packages,
            &annotations.lockfile.crates,
            &annotations.pairred_extras,
            &annotations.metadata.workspace_metadata.tree_metadata,
            include_binaries,
            include_build_scripts,
            are_sources_present,
        )
        .unwrap();

        assert_eq!(context.name, "openssl-sys");
        assert!(context.build_script_attrs.is_none());
        assert_eq!(
            context.targets,
            BTreeSet::from([Rule::Library(TargetAttributes {
                crate_name: "openssl_sys".to_owned(),
                crate_root: Some("src/lib.rs".to_owned()),
                srcs: Glob::new_rust_srcs(!are_sources_present),
            })]),
        );
    }

    #[test]
    fn context_rlib_crate_type() {
        let annotations = crate_type_annotations();

        let package_id = PackageId {
            repr: "registry+https://github.com/rust-lang/crates.io-index#sysinfo@0.22.5".to_owned(),
        };

        let crate_annotation = &annotations.metadata.crates[&package_id];

        let include_binaries = false;
        let include_build_scripts = false;
        let are_sources_present = false;
        let context = CrateContext::new(
            crate_annotation,
            &annotations.metadata.packages,
            &annotations.lockfile.crates,
            &annotations.pairred_extras,
            &annotations.metadata.workspace_metadata.tree_metadata,
            include_binaries,
            include_build_scripts,
            are_sources_present,
        )
        .unwrap();

        assert_eq!(context.name, "sysinfo");
        assert!(context.build_script_attrs.is_none());
        assert_eq!(
            context.targets,
            BTreeSet::from([Rule::Library(TargetAttributes {
                crate_name: "sysinfo".to_owned(),
                crate_root: Some("src/lib.rs".to_owned()),
                srcs: Glob::new_rust_srcs(!are_sources_present),
            })]),
        );
    }

    fn package_context_test(
        set_package: fn(package: &mut Package),
        check_context: fn(context: CrateContext),
    ) {
        let mut annotations = common_annotations();
        let crate_annotation = &annotations.metadata.crates[&PackageId {
            repr: "path+file://{TEMP_DIR}/common#0.1.0".to_owned(),
        }];
        let include_binaries = false;
        let include_build_scripts = false;
        let are_sources_present = false;

        let package = annotations
            .metadata
            .packages
            .get_mut(&crate_annotation.node.id)
            .unwrap();
        set_package(package);

        let context = CrateContext::new(
            crate_annotation,
            &annotations.metadata.packages,
            &annotations.lockfile.crates,
            &annotations.pairred_extras,
            &annotations.metadata.workspace_metadata.tree_metadata,
            include_binaries,
            include_build_scripts,
            are_sources_present,
        )
        .unwrap();

        assert_eq!(context.name, "common");
        check_context(context);
    }

    #[test]
    fn context_with_parsable_license() {
        package_context_test(
            |package| {
                package.license = Some("MIT OR Apache-2.0".to_owned());
            },
            |context| {
                assert_eq!(
                    context.license_ids,
                    BTreeSet::from(["MIT".to_owned(), "Apache-2.0".to_owned(),]),
                );
            },
        );
    }

    #[test]
    fn context_with_unparsable_license() {
        package_context_test(
            |package| {
                package.license = Some("NonSPDXLicenseID".to_owned());
            },
            |context| {
                assert_eq!(context.license_ids, BTreeSet::default());
            },
        );
    }

    #[test]
    fn context_with_license_file() {
        package_context_test(
            |package| {
                package.license_file = Some("LICENSE.txt".into());
            },
            |context| {
                assert_eq!(context.license_file, Some("LICENSE.txt".to_owned()));
            },
        );
    }

    #[test]
    fn context_package_url_with_only_repository() {
        package_context_test(
            |package| {
                package.repository = Some("http://www.repostiory.com/".to_owned());
                package.homepage = None;
            },
            |context| {
                assert_eq!(
                    context.package_url,
                    Some("http://www.repostiory.com/".to_owned())
                );
            },
        );
    }

    #[test]
    fn context_package_url_with_only_homepage() {
        package_context_test(
            |package| {
                package.repository = None;
                package.homepage = Some("http://www.homepage.com/".to_owned());
            },
            |context| {
                assert_eq!(
                    context.package_url,
                    Some("http://www.homepage.com/".to_owned())
                );
            },
        );
    }

    #[test]
    fn context_package_url_prefers_repository() {
        package_context_test(
            |package| {
                package.repository = Some("http://www.repostiory.com/".to_owned());
                package.homepage = Some("http://www.homepage.com/".to_owned());
            },
            |context| {
                assert_eq!(
                    context.package_url,
                    Some("http://www.repostiory.com/".to_owned())
                );
            },
        );
    }

    #[test]
    fn crate_context_features_from_annotations() {
        let mut annotations = common_annotations();

        // Crate a fake feature to track
        let mut select = Select::new();
        select.insert(
            CargoTreeEntry {
                features: BTreeSet::from(["unique_feature".to_owned()]),
                deps: BTreeSet::new(),
            },
            // The common config
            None,
        );
        annotations
            .metadata
            .workspace_metadata
            .tree_metadata
            .insert(
                CrateId::new("common".to_owned(), Version::new(0, 1, 0)),
                select,
            );

        let crate_annotation = &annotations.metadata.crates[&PackageId {
            repr: "path+file://{TEMP_DIR}/common#0.1.0".to_owned(),
        }];
        let include_binaries = false;
        let include_build_scripts = false;
        let are_sources_present = false;

        let context = CrateContext::new(
            crate_annotation,
            &annotations.metadata.packages,
            &annotations.lockfile.crates,
            &annotations.pairred_extras,
            &annotations.metadata.workspace_metadata.tree_metadata,
            include_binaries,
            include_build_scripts,
            are_sources_present,
        )
        .unwrap();

        let mut expected = Select::new();
        expected.insert("unique_feature".to_owned(), None);

        assert_eq!(context.common_attrs.crate_features, expected);
    }

    #[test]
    fn absolute_paths_for_srcs_are_errors() {
        let annotations = Annotations::new(
            crate::test::metadata::abspath(),
            crate::test::lockfile::abspath(),
            crate::config::Config::default(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();

        let crate_annotation = &annotations.metadata.crates[&PackageId {
            repr: "path+file://{TEMP_DIR}/common#0.1.0".to_owned(),
        }];

        let include_binaries = false;
        let include_build_scripts = false;
        let are_sources_present = false;
        let err = CrateContext::new(
            crate_annotation,
            &annotations.metadata.packages,
            &annotations.lockfile.crates,
            &annotations.pairred_extras,
            &annotations.metadata.workspace_metadata.tree_metadata,
            include_binaries,
            include_build_scripts,
            are_sources_present,
        )
        .unwrap_err()
        .to_string();

        assert_eq!(err, "Package \"common\" target \"common\" had an absolute source path \"/dev/null\", which is not supported");
    }
}
