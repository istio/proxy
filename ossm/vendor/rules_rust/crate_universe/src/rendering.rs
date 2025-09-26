//! Tools for rendering and writing BUILD and other Starlark files

mod template_engine;

use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::PathBuf;
use std::str::FromStr;
use std::sync::Arc;

use anyhow::{bail, Context as AnyhowContext, Result};
use itertools::Itertools;

use crate::config::{AliasRule, RenderConfig, VendorMode};
use crate::context::crate_context::{CrateContext, CrateDependency, Rule};
use crate::context::{Context, TargetAttributes};
use crate::rendering::template_engine::TemplateEngine;
use crate::select::Select;
use crate::splicing::default_splicing_package_crate_id;
use crate::utils::starlark::{
    self, Alias, CargoBuildScript, CommonAttrs, Data, ExportsFiles, Filegroup, Glob, Label, Load,
    Package, RustBinary, RustLibrary, RustProcMacro, SelectDict, SelectList, SelectScalar,
    SelectSet, Starlark, TargetCompatibleWith,
};
use crate::utils::target_triple::TargetTriple;
use crate::utils::{self, sanitize_repository_name};

// Configuration remapper used to convert from cfg expressions like "cfg(unix)"
// to platform labels like "@rules_rust//rust/platform:x86_64-unknown-linux-gnu".
pub(crate) type Platforms = BTreeMap<String, BTreeSet<String>>;

pub(crate) struct Renderer {
    config: Arc<RenderConfig>,
    supported_platform_triples: Arc<BTreeSet<TargetTriple>>,
}

impl Renderer {
    pub(crate) fn new(
        config: Arc<RenderConfig>,
        supported_platform_triples: Arc<BTreeSet<TargetTriple>>,
    ) -> Self {
        Self {
            config,
            supported_platform_triples,
        }
    }

    pub(crate) fn render(
        &self,
        context: &Context,
        generator: Option<Label>,
    ) -> Result<BTreeMap<PathBuf, String>> {
        let conditions = Arc::new(context.conditions.clone());
        let engine = self.create_engine(Arc::clone(&conditions));

        let mut output = BTreeMap::new();

        let platforms = self.render_platform_labels(conditions);
        output.extend(self.render_build_files(&engine, context, &platforms)?);
        output.extend(self.render_crates_module(&engine, context, &platforms, generator)?);

        if let Some(vendor_mode) = &self.config.vendor_mode {
            match vendor_mode {
                crate::config::VendorMode::Local => {
                    // Nothing to do for local vendor crate
                }
                crate::config::VendorMode::Remote => {
                    output.extend(self.render_vendor_support_files(&engine, context)?);
                }
            }
        }

        Ok(output)
    }

    pub(crate) fn create_engine(
        &self,
        conditions: Arc<BTreeMap<String, BTreeSet<TargetTriple>>>,
    ) -> TemplateEngine {
        TemplateEngine::new(
            Arc::clone(&self.config),
            Arc::clone(&self.supported_platform_triples),
            Arc::clone(&conditions),
        )
    }

    pub(crate) fn render_platform_labels(
        &self,
        conditions: Arc<BTreeMap<String, BTreeSet<TargetTriple>>>,
    ) -> BTreeMap<String, BTreeSet<String>> {
        conditions
            .iter()
            .map(|(cfg, target_triples)| {
                (
                    cfg.clone(),
                    target_triples
                        .iter()
                        .map(|target_triple| {
                            render_platform_constraint_label(
                                &self.config.platforms_template,
                                target_triple,
                            )
                        })
                        .collect(),
                )
            })
            .collect()
    }

    fn render_crates_module(
        &self,
        engine: &TemplateEngine,
        context: &Context,
        platforms: &Platforms,
        generator: Option<Label>,
    ) -> Result<BTreeMap<PathBuf, String>> {
        let module_label = render_module_label(&self.config.crates_module_template, "defs.bzl")
            .context("Failed to resolve string to module file label")?;
        let module_build_label =
            render_module_label(&self.config.crates_module_template, "BUILD.bazel")
                .context("Failed to resolve string to module file label")?;
        let module_alias_rules_label =
            render_module_label(&self.config.crates_module_template, "alias_rules.bzl")
                .context("Failed to resolve string to module file label")?;

        let mut map = BTreeMap::new();
        map.insert(
            Renderer::label_to_path(&module_label),
            engine.render_module_bzl(context, platforms, generator)?,
        );
        map.insert(
            Renderer::label_to_path(&module_build_label),
            self.render_module_build_file(engine, context)?,
        );
        map.insert(
            Renderer::label_to_path(&module_alias_rules_label),
            include_str!(concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/src/rendering/verbatim/alias_rules.bzl"
            ))
            .to_owned(),
        );

        Ok(map)
    }

    fn render_module_build_file(
        &self,
        engine: &TemplateEngine,
        context: &Context,
    ) -> Result<String> {
        let mut starlark = Vec::new();

        // Banner comment for top of the file.
        let header = engine.render_header()?;
        starlark.push(Starlark::Verbatim(header));

        // Load any `alias_rule`s.
        let mut loads: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
        for alias_rule in Iterator::chain(
            std::iter::once(&self.config.default_alias_rule),
            context
                .workspace_member_deps()
                .iter()
                .flat_map(|dep| &context.crates[&dep.id].alias_rule),
        ) {
            if let Some(bzl) = alias_rule.bzl() {
                loads.entry(bzl).or_default().insert(alias_rule.rule());
            }
        }
        for (bzl, items) in loads {
            starlark.push(Starlark::Load(Load { bzl, items }))
        }

        // Package visibility, exported bzl files.
        let package = Package::default_visibility_public(BTreeSet::new());
        starlark.push(Starlark::Package(package));

        let mut exports_files = ExportsFiles {
            paths: BTreeSet::from(["cargo-bazel.json".to_owned(), "defs.bzl".to_owned()]),
            globs: Glob {
                allow_empty: true,
                include: BTreeSet::from(["*.bazel".to_owned()]),
                exclude: BTreeSet::new(),
            },
        };
        if let Some(VendorMode::Remote) = self.config.vendor_mode {
            exports_files.paths.insert("crates.bzl".to_owned());
        }
        starlark.push(Starlark::ExportsFiles(exports_files));

        let filegroup = Filegroup {
            name: "srcs".to_owned(),
            srcs: Glob {
                allow_empty: true,
                include: BTreeSet::from(["*.bazel".to_owned(), "*.bzl".to_owned()]),
                exclude: BTreeSet::new(),
            },
        };
        starlark.push(Starlark::Filegroup(filegroup));

        // An `alias` for each direct dependency of a workspace member crate.
        let mut dependencies = Vec::new();
        for dep in context.workspace_member_deps() {
            let krate = &context.crates[&dep.id];
            let alias_rule = krate
                .alias_rule
                .as_ref()
                .unwrap_or(&self.config.default_alias_rule);

            if let Some(library_target_name) = &krate.library_target_name {
                let rename = dep.alias.as_ref().unwrap_or(&krate.name);
                dependencies.push(Alias {
                    rule: alias_rule.rule(),
                    // If duplicates exist, include version to disambiguate them.
                    name: if context.has_duplicate_workspace_member_dep(&dep) {
                        format!("{}-{}", rename, krate.version)
                    } else {
                        rename.clone()
                    },
                    actual: self.crate_label(
                        &krate.name,
                        &krate.version.to_string(),
                        library_target_name,
                    ),
                    tags: BTreeSet::from(["manual".to_owned()]),
                });
            }

            for (alias, target) in &krate.extra_aliased_targets {
                dependencies.push(Alias {
                    rule: alias_rule.rule(),
                    name: alias.clone(),
                    actual: self.crate_label(&krate.name, &krate.version.to_string(), target),
                    tags: BTreeSet::from(["manual".to_owned()]),
                });
            }
        }

        let duplicates: Vec<_> = dependencies
            .iter()
            .map(|alias| &alias.name)
            .duplicates()
            .sorted()
            .collect();

        assert!(
            duplicates.is_empty(),
            "Found duplicate aliases that must be changed (Check your `extra_aliased_targets`): {:#?}",
            duplicates
        );

        if !dependencies.is_empty() {
            let comment = "# Workspace Member Dependencies".to_owned();
            starlark.push(Starlark::Verbatim(comment));
            starlark.extend(dependencies.into_iter().map(Starlark::Alias));
        }

        // An `alias` for each binary dependency.
        let mut binaries = Vec::new();
        for crate_id in &context.binary_crates {
            let krate = &context.crates[crate_id];
            for rule in &krate.targets {
                if let Rule::Binary(bin) = rule {
                    binaries.push(Alias {
                        rule: AliasRule::default().rule(),
                        // If duplicates exist, include version to disambiguate them.
                        name: if context.has_duplicate_binary_crate(crate_id) {
                            format!("{}-{}__{}", krate.name, krate.version, bin.crate_name)
                        } else {
                            format!("{}__{}", krate.name, bin.crate_name)
                        },
                        actual: self.crate_label(
                            &krate.name,
                            &krate.version.to_string(),
                            &format!("{}__bin", bin.crate_name),
                        ),
                        tags: BTreeSet::from(["manual".to_owned()]),
                    });
                }
            }
        }
        if !binaries.is_empty() {
            let comment = "# Binaries".to_owned();
            starlark.push(Starlark::Verbatim(comment));
            starlark.extend(binaries.into_iter().map(Starlark::Alias));
        }

        let starlark = starlark::serialize(&starlark)?;
        Ok(starlark)
    }

    fn render_build_files(
        &self,
        engine: &TemplateEngine,
        context: &Context,
        platforms: &Platforms,
    ) -> Result<BTreeMap<PathBuf, String>> {
        let default_splicing_package_id = default_splicing_package_crate_id();
        context
            .crates
            .keys()
            // Do not render the default splicing package
            .filter(|id| *id != &default_splicing_package_id)
            // Do not render local packages
            .filter(|id| !context.workspace_members.contains_key(id))
            .map(|id| {
                let label = match render_build_file_template(
                    &self.config.build_file_template,
                    &id.name,
                    &id.version.to_string(),
                ) {
                    Ok(label) => label,
                    Err(e) => bail!(e),
                };

                let filename = Renderer::label_to_path(&label);
                let content = self.render_one_build_file(engine, platforms, &context.crates[id])?;
                Ok((filename, content))
            })
            .collect()
    }

    pub(crate) fn render_one_build_file(
        &self,
        engine: &TemplateEngine,
        platforms: &Platforms,
        krate: &CrateContext,
    ) -> Result<String> {
        let mut starlark = Vec::new();

        // Banner comment for top of the file.
        let header = engine.render_header()?;
        starlark.push(Starlark::Verbatim(header));

        // Loads: map of bzl file to set of items imported from that file. These
        // get inserted into `starlark` at the bottom of this function.
        let mut loads: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
        let mut load = |bzl: &str, item: &str| {
            loads
                .entry(bzl.to_owned())
                .or_default()
                .insert(item.to_owned())
        };

        let disable_visibility = "# buildifier: disable=bzl-visibility".to_owned();
        starlark.push(Starlark::Verbatim(disable_visibility));
        starlark.push(Starlark::Load(Load {
            bzl: "@rules_rust//crate_universe/private:selects.bzl".to_owned(),
            items: BTreeSet::from(["selects".to_owned()]),
        }));

        if self.config.generate_rules_license_metadata {
            let has_license_ids = !krate.license_ids.is_empty();
            let mut package_metadata = BTreeSet::from([Label::Relative {
                target: "package_info".to_owned(),
            }]);

            starlark.push(Starlark::Load(Load {
                bzl: "@rules_license//rules:package_info.bzl".to_owned(),
                items: BTreeSet::from(["package_info".to_owned()]),
            }));

            if has_license_ids {
                starlark.push(Starlark::Load(Load {
                    bzl: "@rules_license//rules:license.bzl".to_owned(),
                    items: BTreeSet::from(["license".to_owned()]),
                }));
                package_metadata.insert(Label::Relative {
                    target: "license".to_owned(),
                });
            }

            let package = Package::default_visibility_public(package_metadata);
            starlark.push(Starlark::Package(package));

            starlark.push(Starlark::PackageInfo(starlark::PackageInfo {
                name: "package_info".to_owned(),
                package_name: krate.name.clone(),
                package_url: krate.package_url.clone().unwrap_or_default(),
                package_version: krate.version.to_string(),
            }));

            if has_license_ids {
                let mut license_kinds = BTreeSet::new();

                krate.license_ids.clone().into_iter().for_each(|lic| {
                    license_kinds.insert("@rules_license//licenses/spdx:".to_owned() + &lic);
                });

                starlark.push(Starlark::License(starlark::License {
                    name: "license".to_owned(),
                    license_kinds,
                    license_text: krate.license_file.clone().unwrap_or_default(),
                }));
            }
        } else {
            // Package visibility.
            let package = Package::default_visibility_public(BTreeSet::new());
            starlark.push(Starlark::Package(package));
        }

        for rule in &krate.targets {
            if let Some(override_target) = krate.override_targets.get(rule.override_target_key()) {
                starlark.push(Starlark::Alias(Alias {
                    rule: AliasRule::default().rule(),
                    name: rule.crate_name().to_owned(),
                    actual: override_target.clone(),
                    tags: BTreeSet::from(["manual".to_owned()]),
                }));
            } else {
                match rule {
                    Rule::BuildScript(target) => {
                        load("@rules_rust//cargo:defs.bzl", "cargo_build_script");
                        let cargo_build_script =
                            self.make_cargo_build_script(platforms, krate, target)?;
                        starlark.push(Starlark::CargoBuildScript(cargo_build_script));
                        starlark.push(Starlark::Alias(Alias {
                            rule: AliasRule::default().rule(),
                            name: target.crate_name.clone(),
                            actual: Label::from_str("_bs").unwrap(),
                            tags: BTreeSet::from(["manual".to_owned()]),
                        }));
                    }
                    Rule::ProcMacro(target) => {
                        load("@rules_rust//rust:defs.bzl", "rust_proc_macro");
                        let rust_proc_macro =
                            self.make_rust_proc_macro(platforms, krate, target)?;
                        starlark.push(Starlark::RustProcMacro(rust_proc_macro));
                    }
                    Rule::Library(target) => {
                        load("@rules_rust//rust:defs.bzl", "rust_library");
                        let rust_library = self.make_rust_library(platforms, krate, target)?;
                        starlark.push(Starlark::RustLibrary(rust_library));
                    }
                    Rule::Binary(target) => {
                        load("@rules_rust//rust:defs.bzl", "rust_binary");
                        let rust_binary = self.make_rust_binary(platforms, krate, target)?;
                        starlark.push(Starlark::RustBinary(rust_binary));
                    }
                }
            }
        }

        if let Some(additive_build_file_content) = &krate.additive_build_file_content {
            let comment = "# Additive BUILD file content".to_owned();
            starlark.push(Starlark::Verbatim(comment));
            starlark.push(Starlark::Verbatim(additive_build_file_content.clone()));
        }

        // Insert all the loads immediately after the header banner comment.
        let loads = loads
            .into_iter()
            .map(|(bzl, items)| Starlark::Load(Load { bzl, items }));
        starlark.splice(1..1, loads);

        let starlark = starlark::serialize(&starlark)?;
        Ok(starlark)
    }

    fn make_cargo_build_script(
        &self,
        platforms: &Platforms,
        krate: &CrateContext,
        target: &TargetAttributes,
    ) -> Result<CargoBuildScript> {
        let attrs = krate.build_script_attrs.as_ref();

        const COMPILE_DATA_GLOB_EXCLUDES: &[&str] = &["**/*.rs"];

        Ok(CargoBuildScript {
            // Because `cargo_build_script` does some invisible target name
            // mutating to determine the package and crate name for a build
            // script, the Bazel target name of any build script cannot be the
            // Cargo canonical name of "cargo_build_script" without losing out
            // on having certain Cargo environment variables set.
            //
            // Do not change this name to "cargo_build_script".
            //
            // This is set to a short name to avoid long path name issues on windows.
            name: "_bs".to_string(),
            aliases: SelectDict::new(self.make_aliases(krate, true, false), platforms),
            build_script_env: SelectDict::new(
                attrs
                    .map(|attrs| attrs.build_script_env.clone())
                    .unwrap_or_default(),
                platforms,
            ),
            use_default_shell_env: krate
                .build_script_attrs
                .as_ref()
                .and_then(|a| a.use_default_shell_env),
            compile_data: make_data_with_exclude(
                platforms,
                attrs
                    .map(|attrs| attrs.compile_data_glob.clone())
                    .unwrap_or_default(),
                COMPILE_DATA_GLOB_EXCLUDES
                    .iter()
                    .map(|&pattern| pattern.to_owned())
                    .collect(),
                attrs
                    .map(|attrs| attrs.compile_data.clone())
                    .unwrap_or_default(),
            ),
            crate_features: SelectSet::new(krate.common_attrs.crate_features.clone(), platforms),
            crate_name: utils::sanitize_module_name(&target.crate_name),
            crate_root: target.crate_root.clone(),
            data: make_data(
                platforms,
                attrs
                    .map(|attrs| attrs.data_glob.clone())
                    .unwrap_or_default(),
                attrs.map(|attrs| attrs.data.clone()).unwrap_or_default(),
            ),
            deps: SelectSet::new(
                self.make_deps(
                    attrs.map(|attrs| attrs.deps.clone()).unwrap_or_default(),
                    attrs
                        .map(|attrs| attrs.extra_deps.clone())
                        .unwrap_or_default(),
                ),
                platforms,
            ),
            link_deps: SelectSet::new(
                self.make_deps(
                    attrs
                        .map(|attrs| attrs.link_deps.clone())
                        .unwrap_or_default(),
                    attrs
                        .map(|attrs| attrs.extra_link_deps.clone())
                        .unwrap_or_default(),
                ),
                platforms,
            ),
            edition: krate.common_attrs.edition.clone(),
            linker_script: krate.common_attrs.linker_script.clone(),
            links: attrs.and_then(|attrs| attrs.links.clone()),
            pkg_name: Some(krate.name.clone()),
            proc_macro_deps: SelectSet::new(
                self.make_deps(
                    attrs
                        .map(|attrs| attrs.proc_macro_deps.clone())
                        .unwrap_or_default(),
                    attrs
                        .map(|attrs| attrs.extra_proc_macro_deps.clone())
                        .unwrap_or_default(),
                ),
                platforms,
            ),
            rundir: SelectScalar::new(
                attrs.map(|attrs| attrs.rundir.clone()).unwrap_or_default(),
                platforms,
            ),
            rustc_env: SelectDict::new(
                attrs
                    .map(|attrs| attrs.rustc_env.clone())
                    .unwrap_or_default(),
                platforms,
            ),
            rustc_env_files: SelectSet::new(
                attrs
                    .map(|attrs| attrs.rustc_env_files.clone())
                    .unwrap_or_default(),
                platforms,
            ),
            rustc_flags: SelectList::new(
                // In most cases, warnings in 3rd party crates are not
                // interesting as they're out of the control of consumers. The
                // flag here silences warnings. For more details see:
                // https://doc.rust-lang.org/rustc/lints/levels.html
                Select::merge(
                    Select::from_value(Vec::from(["--cap-lints=allow".to_owned()])),
                    attrs
                        .map(|attrs| attrs.rustc_flags.clone())
                        .unwrap_or_default(),
                ),
                platforms,
            ),
            srcs: target.srcs.clone(),
            tags: {
                let mut tags = BTreeSet::from_iter(krate.common_attrs.tags.iter().cloned());
                tags.insert("cargo-bazel".to_owned());
                tags.insert("manual".to_owned());
                tags.insert("noclippy".to_owned());
                tags.insert("norustfmt".to_owned());
                tags.insert(format!("crate-name={}", krate.name));
                tags
            },
            tools: SelectSet::new(
                attrs.map(|attrs| attrs.tools.clone()).unwrap_or_default(),
                platforms,
            ),
            toolchains: attrs.map_or_else(BTreeSet::new, |attrs| attrs.toolchains.clone()),
            version: krate.common_attrs.version.clone(),
            visibility: BTreeSet::from(["//visibility:private".to_owned()]),
        })
    }

    fn make_rust_proc_macro(
        &self,
        platforms: &Platforms,
        krate: &CrateContext,
        target: &TargetAttributes,
    ) -> Result<RustProcMacro> {
        Ok(RustProcMacro {
            name: target.crate_name.clone(),
            deps: SelectSet::new(
                self.make_deps(
                    krate.common_attrs.deps.clone(),
                    krate.common_attrs.extra_deps.clone(),
                ),
                platforms,
            ),
            proc_macro_deps: SelectSet::new(
                self.make_deps(
                    krate.common_attrs.proc_macro_deps.clone(),
                    krate.common_attrs.extra_proc_macro_deps.clone(),
                ),
                platforms,
            ),
            aliases: SelectDict::new(self.make_aliases(krate, false, false), platforms),
            common: self.make_common_attrs(platforms, krate, target)?,
        })
    }

    fn make_rust_library(
        &self,
        platforms: &Platforms,
        krate: &CrateContext,
        target: &TargetAttributes,
    ) -> Result<RustLibrary> {
        Ok(RustLibrary {
            name: target.crate_name.clone(),
            deps: SelectSet::new(
                self.make_deps(
                    krate.common_attrs.deps.clone(),
                    krate.common_attrs.extra_deps.clone(),
                ),
                platforms,
            ),
            proc_macro_deps: SelectSet::new(
                self.make_deps(
                    krate.common_attrs.proc_macro_deps.clone(),
                    krate.common_attrs.extra_proc_macro_deps.clone(),
                ),
                platforms,
            ),
            aliases: SelectDict::new(self.make_aliases(krate, false, false), platforms),
            common: self.make_common_attrs(platforms, krate, target)?,
            disable_pipelining: krate.disable_pipelining,
        })
    }

    fn make_rust_binary(
        &self,
        platforms: &Platforms,
        krate: &CrateContext,
        target: &TargetAttributes,
    ) -> Result<RustBinary> {
        Ok(RustBinary {
            name: format!("{}__bin", target.crate_name),
            deps: {
                let mut deps = self.make_deps(
                    krate.common_attrs.deps.clone(),
                    krate.common_attrs.extra_deps.clone(),
                );
                if let Some(library_target_name) = &krate.library_target_name {
                    deps.insert(
                        Label::from_str(&format!(":{library_target_name}")).unwrap(),
                        None,
                    );
                }
                SelectSet::new(deps, platforms)
            },
            proc_macro_deps: SelectSet::new(
                self.make_deps(
                    krate.common_attrs.proc_macro_deps.clone(),
                    krate.common_attrs.extra_proc_macro_deps.clone(),
                ),
                platforms,
            ),
            aliases: SelectDict::new(self.make_aliases(krate, false, false), platforms),
            common: self.make_common_attrs(platforms, krate, target)?,
        })
    }

    fn make_common_attrs(
        &self,
        platforms: &Platforms,
        krate: &CrateContext,
        target: &TargetAttributes,
    ) -> Result<CommonAttrs> {
        Ok(CommonAttrs {
            compile_data: make_data(
                platforms,
                krate.common_attrs.compile_data_glob.clone(),
                krate.common_attrs.compile_data.clone(),
            ),
            crate_features: SelectSet::new(krate.common_attrs.crate_features.clone(), platforms),
            crate_root: target.crate_root.clone(),
            data: make_data(
                platforms,
                krate.common_attrs.data_glob.clone(),
                krate.common_attrs.data.clone(),
            ),
            edition: krate.common_attrs.edition.clone(),
            linker_script: krate.common_attrs.linker_script.clone(),
            rustc_env: SelectDict::new(krate.common_attrs.rustc_env.clone(), platforms),
            rustc_env_files: SelectSet::new(krate.common_attrs.rustc_env_files.clone(), platforms),
            rustc_flags: SelectList::new(
                // In most cases, warnings in 3rd party crates are not
                // interesting as they're out of the control of consumers. The
                // flag here silences warnings. For more details see:
                // https://doc.rust-lang.org/rustc/lints/levels.html
                Select::merge(
                    Select::from_value(Vec::from(["--cap-lints=allow".to_owned()])),
                    krate.common_attrs.rustc_flags.clone(),
                ),
                platforms,
            ),
            srcs: target.srcs.clone(),
            tags: {
                let mut tags = BTreeSet::from_iter(krate.common_attrs.tags.iter().cloned());
                tags.insert("cargo-bazel".to_owned());
                tags.insert("manual".to_owned());
                tags.insert("noclippy".to_owned());
                tags.insert("norustfmt".to_owned());
                tags.insert(format!("crate-name={}", krate.name));
                tags
            },
            target_compatible_with: self.config.generate_target_compatible_with.then(|| {
                TargetCompatibleWith::new(
                    self.supported_platform_triples
                        .iter()
                        .map(|target_triple| {
                            render_platform_constraint_label(
                                &self.config.platforms_template,
                                target_triple,
                            )
                        })
                        .collect(),
                )
            }),
            version: krate.common_attrs.version.clone(),
        })
    }

    /// Filter a crate's dependencies to only ones with aliases
    fn make_aliases(
        &self,
        krate: &CrateContext,
        build: bool,
        include_dev: bool,
    ) -> Select<BTreeMap<Label, String>> {
        let mut dependency_selects = Vec::new();
        if build {
            if let Some(build_script_attrs) = &krate.build_script_attrs {
                dependency_selects.push(&build_script_attrs.deps);
                dependency_selects.push(&build_script_attrs.proc_macro_deps);
            }
        } else {
            dependency_selects.push(&krate.common_attrs.deps);
            dependency_selects.push(&krate.common_attrs.proc_macro_deps);
            if include_dev {
                dependency_selects.push(&krate.common_attrs.deps_dev);
                dependency_selects.push(&krate.common_attrs.proc_macro_deps_dev);
            }
        }

        let mut aliases: Select<BTreeMap<Label, String>> = Select::default();
        for dependency_select in dependency_selects.iter() {
            for (configuration, dependency) in dependency_select.items().into_iter() {
                if let Some(alias) = &dependency.alias {
                    let label = self.crate_label(
                        &dependency.id.name,
                        &dependency.id.version.to_string(),
                        &dependency.target,
                    );
                    aliases.insert((label, alias.clone()), configuration.clone());
                }
            }
        }
        aliases
    }

    fn make_deps(
        &self,
        deps: Select<BTreeSet<CrateDependency>>,
        extra_deps: Select<BTreeSet<Label>>,
    ) -> Select<BTreeSet<Label>> {
        Select::merge(
            deps.map(|dep| {
                self.crate_label(&dep.id.name, &dep.id.version.to_string(), &dep.target)
            }),
            extra_deps,
        )
    }

    fn render_vendor_support_files(
        &self,
        engine: &TemplateEngine,
        context: &Context,
    ) -> Result<BTreeMap<PathBuf, String>> {
        let module_label = render_module_label(&self.config.crates_module_template, "crates.bzl")
            .context("Failed to resolve string to module file label")?;

        let mut map = BTreeMap::new();
        map.insert(
            Renderer::label_to_path(&module_label),
            engine.render_vendor_module_file(context)?,
        );

        Ok(map)
    }

    fn label_to_path(label: &Label) -> PathBuf {
        match &label.package() {
            Some(package) if !package.is_empty() => {
                PathBuf::from(format!("{}/{}", package, label.target()))
            }
            Some(_) | None => PathBuf::from(label.target()),
        }
    }

    fn crate_label(&self, name: &str, version: &str, target: &str) -> Label {
        Label::from_str(&sanitize_repository_name(&render_crate_bazel_label(
            &self.config.crate_label_template,
            &self.config.repository_name,
            name,
            version,
            target,
        )))
        .unwrap()
    }
}

/// Write a set of [crate::context::crate_context::CrateContext] to disk.
pub(crate) fn write_outputs(outputs: BTreeMap<PathBuf, String>, dry_run: bool) -> Result<()> {
    if dry_run {
        for (path, content) in outputs {
            println!(
                "==============================================================================="
            );
            println!("{}", path.display());
            println!(
                "==============================================================================="
            );
            println!("{content}\n");
        }
    } else {
        for (path, content) in outputs {
            // Ensure the output directory exists
            fs::create_dir_all(
                path.parent()
                    .expect("All file paths should have valid directories"),
            )?;
            fs::write(&path, content.as_bytes())
                .context(format!("Failed to write file to disk: {}", path.display()))?;
        }
    }

    Ok(())
}

/// Render the Bazel label of a crate
pub(crate) fn render_crate_bazel_label(
    template: &str,
    repository_name: &str,
    name: &str,
    version: &str,
    target: &str,
) -> String {
    template
        .replace("{repository}", repository_name)
        .replace("{name}", name)
        .replace("{version}", version)
        .replace("{target}", target)
}

/// Render the Bazel label of a crate
pub(crate) fn render_crate_bazel_repository(
    template: &str,
    repository_name: &str,
    name: &str,
    version: &str,
) -> String {
    template
        .replace("{repository}", repository_name)
        .replace("{name}", name)
        .replace("{version}", version)
}

/// Render the Bazel label of a crate
pub(crate) fn render_crate_build_file(template: &str, name: &str, version: &str) -> String {
    template
        .replace("{name}", name)
        .replace("{version}", version)
}

/// Render the Bazel label of a vendor module label
pub(crate) fn render_module_label(template: &str, name: &str) -> Result<Label> {
    Label::from_str(&template.replace("{file}", name))
}

/// Render the Bazel label of a platform triple
fn render_platform_constraint_label(template: &str, target_triple: &TargetTriple) -> String {
    template.replace("{triple}", &target_triple.to_bazel())
}

fn render_build_file_template(template: &str, name: &str, version: &str) -> Result<Label> {
    Label::from_str(
        &template
            .replace("{name}", name)
            .replace("{version}", version),
    )
}

fn make_data_with_exclude(
    platforms: &Platforms,
    include: BTreeSet<String>,
    exclude: BTreeSet<String>,
    select: Select<BTreeSet<Label>>,
) -> Data {
    const COMMON_GLOB_EXCLUDES: &[&str] = &[
        "**/* *",
        "BUILD.bazel",
        "BUILD",
        "WORKSPACE.bazel",
        "WORKSPACE",
        ".tmp_git_root/**/*",
    ];

    Data {
        glob: Glob {
            allow_empty: true,
            include,
            exclude: COMMON_GLOB_EXCLUDES
                .iter()
                .map(|&glob| glob.to_owned())
                .chain(exclude)
                .collect(),
        },
        select: SelectSet::new(select, platforms),
    }
}

fn make_data(
    platforms: &Platforms,
    glob: BTreeSet<String>,
    select: Select<BTreeSet<Label>>,
) -> Data {
    make_data_with_exclude(platforms, glob, BTreeSet::new(), select)
}

#[cfg(test)]
mod test {
    use super::*;

    use camino::Utf8Path;
    use indoc::indoc;

    use crate::config::{Config, CrateId};
    use crate::context::{BuildScriptAttributes, CommonAttributes};
    use crate::metadata::Annotations;
    use crate::test;
    use crate::utils::normalize_cargo_file_paths;

    const VERSION_ZERO_ONE_ZERO: semver::Version = semver::Version::new(0, 1, 0);

    fn mock_target_attributes() -> TargetAttributes {
        TargetAttributes {
            crate_name: "mock_crate".to_owned(),
            crate_root: Some("src/root.rs".to_owned()),
            ..TargetAttributes::default()
        }
    }

    fn mock_render_config(vendor_mode: Option<VendorMode>) -> Arc<RenderConfig> {
        Arc::new(RenderConfig {
            repository_name: "test_rendering".to_owned(),
            regen_command: "cargo_bazel_regen_command".to_owned(),
            vendor_mode,
            ..RenderConfig::default()
        })
    }

    fn mock_supported_platform_triples() -> Arc<BTreeSet<TargetTriple>> {
        Arc::new(BTreeSet::from([
            TargetTriple::from_bazel("aarch64-apple-darwin".to_owned()),
            TargetTriple::from_bazel("aarch64-apple-ios".to_owned()),
            TargetTriple::from_bazel("aarch64-linux-android".to_owned()),
            TargetTriple::from_bazel("aarch64-pc-windows-msvc".to_owned()),
            TargetTriple::from_bazel("aarch64-unknown-linux-gnu".to_owned()),
            TargetTriple::from_bazel("arm-unknown-linux-gnueabi".to_owned()),
            TargetTriple::from_bazel("armv7-unknown-linux-gnueabi".to_owned()),
            TargetTriple::from_bazel("i686-apple-darwin".to_owned()),
            TargetTriple::from_bazel("i686-linux-android".to_owned()),
            TargetTriple::from_bazel("i686-pc-windows-msvc".to_owned()),
            TargetTriple::from_bazel("i686-unknown-freebsd".to_owned()),
            TargetTriple::from_bazel("i686-unknown-linux-gnu".to_owned()),
            TargetTriple::from_bazel("powerpc-unknown-linux-gnu".to_owned()),
            TargetTriple::from_bazel("s390x-unknown-linux-gnu".to_owned()),
            TargetTriple::from_bazel("wasm32-unknown-unknown".to_owned()),
            TargetTriple::from_bazel("wasm32-wasip1".to_owned()),
            TargetTriple::from_bazel("x86_64-apple-darwin".to_owned()),
            TargetTriple::from_bazel("x86_64-apple-ios".to_owned()),
            TargetTriple::from_bazel("x86_64-linux-android".to_owned()),
            TargetTriple::from_bazel("x86_64-pc-windows-msvc".to_owned()),
            TargetTriple::from_bazel("x86_64-unknown-freebsd".to_owned()),
            TargetTriple::from_bazel("x86_64-unknown-linux-gnu".to_owned()),
        ]))
    }

    #[test]
    fn render_rust_library() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        assert!(build_file_content.contains("rust_library("));
        assert!(build_file_content.contains("name = \"mock_crate\""));
        assert!(build_file_content.contains("\"crate-name=mock_crate\""));
    }

    #[test]
    fn test_disable_pipelining() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: true,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        assert!(build_file_content.contains("disable_pipelining = True"));
    }

    #[test]
    fn render_cargo_build_script() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);

        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::BuildScript(TargetAttributes {
                    crate_name: "build_script_build".to_owned(),
                    crate_root: Some("build.rs".to_owned()),
                    ..TargetAttributes::default()
                })]),
                // Build script attributes are required.
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: Some(BuildScriptAttributes::default()),
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        assert!(
            build_file_content.contains("cargo_build_script("),
            "```\n{}```\n",
            build_file_content
        );
        assert!(
            build_file_content.contains("name = \"build_script_build\""),
            "```\n{}```\n",
            build_file_content
        );
        assert!(
            build_file_content.contains("\"crate-name=mock_crate\""),
            "```\n{}```\n",
            build_file_content
        );
        assert!(
            build_file_content.contains("compile_data = glob("),
            "```\n{}```\n",
            build_file_content
        );
        assert!(
            !build_file_content.contains("use_default_shell_env ="),
            "```\n{}```\n",
            build_file_content
        );

        // Ensure `cargo_build_script` requirements are met
        assert!(build_file_content.contains("name = \"_bs\""));
    }

    #[test]
    fn render_cargo_build_script_complex() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);

        let attrs = BuildScriptAttributes {
            use_default_shell_env: Some(1),
            ..BuildScriptAttributes::default()
        };

        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::BuildScript(TargetAttributes {
                    crate_name: "build_script_build".to_owned(),
                    crate_root: Some("build.rs".to_owned()),
                    ..TargetAttributes::default()
                })]),
                // Build script attributes are required.
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: Some(attrs),
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        assert!(
            build_file_content.contains("cargo_build_script("),
            "```\n{}```\n",
            build_file_content
        );
        assert!(
            build_file_content.contains("name = \"build_script_build\""),
            "```\n{}```\n",
            build_file_content
        );
        assert!(
            build_file_content.contains("\"crate-name=mock_crate\""),
            "```\n{}```\n",
            build_file_content
        );
        assert!(
            build_file_content.contains("compile_data = glob("),
            "```\n{}```\n",
            build_file_content
        );
        assert!(
            build_file_content.contains("use_default_shell_env = 1"),
            "```\n{}```\n",
            build_file_content
        );

        // Ensure `cargo_build_script` requirements are met
        assert!(build_file_content.contains("name = \"_bs\""));
    }

    #[test]
    fn render_proc_macro() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::ProcMacro(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        assert!(build_file_content.contains("rust_proc_macro("));
        assert!(build_file_content.contains("name = \"mock_crate\""));
        assert!(build_file_content.contains("\"crate-name=mock_crate\""));
    }

    #[test]
    fn render_binary() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Binary(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        assert!(build_file_content.contains("rust_binary("));
        assert!(build_file_content.contains("name = \"mock_crate__bin\""));
        assert!(build_file_content.contains("\"crate-name=mock_crate\""));
    }

    #[test]
    fn render_additive_build_contents() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Binary(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: Some(
                    "# Hello World from additive section!".to_owned(),
                ),
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        assert!(build_file_content.contains("# Hello World from additive section!"));
    }

    #[test]
    fn render_aliases() {
        let config = Config {
            generate_binaries: true,
            ..Config::default()
        };
        let annotations = Annotations::new(
            test::metadata::alias(),
            test::lockfile::alias(),
            config,
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();
        let context = Context::new(annotations, false).unwrap();

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output.get(&PathBuf::from("BUILD.bazel")).unwrap();

        assert!(build_file_content.contains(r#"name = "names-0.12.1-dev__names","#));
        assert!(build_file_content.contains(r#"name = "names-0.13.0__names","#));
    }

    #[test]
    fn render_crate_repositories() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let defs_module = output.get(&PathBuf::from("defs.bzl")).unwrap();

        assert!(defs_module.contains("def crate_repositories():"));
    }

    #[test]
    fn remote_remote_vendor_mode() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        // Enable remote vendor mode
        let renderer = Renderer::new(
            mock_render_config(Some(VendorMode::Remote)),
            mock_supported_platform_triples(),
        );
        let output = renderer.render(&context, None).unwrap();

        let defs_module = output.get(&PathBuf::from("defs.bzl")).unwrap();
        assert!(defs_module.contains("def crate_repositories():"));

        let crates_module = output.get(&PathBuf::from("crates.bzl")).unwrap();
        assert!(crates_module.contains("def crate_repositories():"));
    }

    #[test]
    fn remote_local_vendor_mode() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        // Enable local vendor mode
        let renderer = Renderer::new(
            mock_render_config(Some(VendorMode::Local)),
            mock_supported_platform_triples(),
        );
        let output = renderer.render(&context, None).unwrap();

        // Local vendoring does not produce a `crate_repositories` macro
        let defs_module = output.get(&PathBuf::from("defs.bzl")).unwrap();
        assert!(!defs_module.contains("def crate_repositories():"));

        // Local vendoring does not produce a `crates.bzl` file.
        assert!(!output.contains_key(&PathBuf::from("crates.bzl")));
    }

    #[test]
    fn duplicate_rustc_flags() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);

        let rustc_flags = vec![
            "-l".to_owned(),
            "dylib=ssl".to_owned(),
            "-l".to_owned(),
            "dylib=crypto".to_owned(),
        ];

        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes {
                    rustc_flags: Select::from_value(rustc_flags.clone()),
                    ..CommonAttributes::default()
                },
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        // Enable local vendor mode
        let renderer = Renderer::new(
            mock_render_config(Some(VendorMode::Local)),
            mock_supported_platform_triples(),
        );
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        // Strip all spaces from the generated BUILD file and ensure it has the flags
        // represented by `rustc_flags` in the same order.
        assert!(build_file_content.replace(' ', "").contains(
            &rustc_flags
                .iter()
                .map(|s| format!("\"{s}\","))
                .collect::<Vec<String>>()
                .join("\n")
        ));
    }

    #[test]
    fn test_render_build_file_deps() {
        let config: Config = serde_json::from_value(serde_json::json!({
            "generate_binaries": false,
            "generate_build_scripts": false,
            "rendering": {
                "repository_name": "multi_cfg_dep",
                "regen_command": "bazel test //crate_universe:unit_test",
            },
            "supported_platform_triples": [
                "x86_64-apple-darwin",
                "x86_64-unknown-linux-gnu",
                "aarch64-apple-darwin",
                "aarch64-unknown-linux-gnu",
            ],
        }))
        .unwrap();
        let metadata = test::metadata::multi_cfg_dep();
        let lockfile = test::lockfile::multi_cfg_dep();

        let annotations = Annotations::new(
            metadata,
            lockfile,
            config.clone(),
            Utf8Path::new("/tmp/bazelworkspace"),
        )
        .unwrap();
        let context = Context::new(annotations, false).unwrap();

        let renderer = Renderer::new(
            Arc::new(config.rendering),
            Arc::new(config.supported_platform_triples),
        );
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.cpufeatures-0.2.7.bazel"))
            .unwrap();

        // This is unfortunately somewhat brittle. Alas. Ultimately we wish to demonstrate that the
        // original cfg(...) strings are preserved in the `deps` list for ease of debugging.
        let expected = indoc! {r#"
            deps = select({
                "@rules_rust//rust/platform:aarch64-apple-darwin": [
                    "@multi_cfg_dep__libc-0.2.117//:libc",  # cfg(all(target_arch = "aarch64", target_vendor = "apple"))
                ],
                "@rules_rust//rust/platform:aarch64-unknown-linux-gnu": [
                    "@multi_cfg_dep__libc-0.2.117//:libc",  # cfg(all(target_arch = "aarch64", target_os = "linux"))
                ],
                "//conditions:default": [],
            }),
        "#};

        assert!(
            build_file_content.contains(&expected.replace('\n', "\n    ")),
            "{}",
            build_file_content,
        );
    }

    #[test]
    fn crate_features_by_target() {
        let mut context = Context {
            conditions: mock_supported_platform_triples()
                .iter()
                .map(|platform| (platform.to_bazel(), BTreeSet::from([platform.clone()])))
                .collect(),
            ..Context::default()
        };
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        let mut crate_features: Select<BTreeSet<String>> = Select::default();
        crate_features.insert("foo".to_owned(), Some("aarch64-apple-darwin".to_owned()));
        crate_features.insert("bar".to_owned(), None);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes {
                    crate_features,
                    ..CommonAttributes::default()
                },
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let renderer = Renderer::new(mock_render_config(None), mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();
        let expected = indoc! {r#"
            crate_features = [
                "bar",
            ] + select({
                "@rules_rust//rust/platform:aarch64-apple-darwin": [
                    "foo",  # aarch64-apple-darwin
                ],
                "//conditions:default": [],
            }),
        "#};
        assert!(build_file_content
            .replace(' ', "")
            .contains(&expected.replace(' ', "")));
    }

    #[test]
    fn crate_package_metadata_without_license_ids() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: Some("http://www.mock_crate.com/".to_owned()),
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let mut render_config = mock_render_config(None);
        Arc::get_mut(&mut render_config)
            .unwrap()
            .generate_rules_license_metadata = true;
        let renderer = Renderer::new(render_config, mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        let expected = indoc! {r#"
            package(
                default_package_metadata = [":package_info"],
                default_visibility = ["//visibility:public"],
            )

            package_info(
                name = "package_info",
                package_name = "mock_crate",
                package_version = "0.1.0",
                package_url = "http://www.mock_crate.com/",
            )
        "#};
        assert!(build_file_content
            .replace(' ', "")
            .contains(&expected.replace(' ', "")));
    }

    #[test]
    fn crate_package_metadata_with_license_ids() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: Some("http://www.mock_crate.com/".to_owned()),
                license_ids: BTreeSet::from(["Apache-2.0".to_owned(), "MIT".to_owned()]),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                repository: None,
                license: None,
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let mut render_config = mock_render_config(None);
        Arc::get_mut(&mut render_config)
            .unwrap()
            .generate_rules_license_metadata = true;
        let renderer = Renderer::new(render_config, mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        let expected = indoc! {r#"
            package(
                default_package_metadata = [
                    ":license",
                    ":package_info",
                ],
                default_visibility = ["//visibility:public"],
            )

            package_info(
                name = "package_info",
                package_name = "mock_crate",
                package_version = "0.1.0",
                package_url = "http://www.mock_crate.com/",
            )

            license(
                name = "license",
                license_kinds = [
                    "@rules_license//licenses/spdx:Apache-2.0",
                    "@rules_license//licenses/spdx:MIT",
                ],
            )
        "#};
        assert!(build_file_content
            .replace(' ', "")
            .contains(&expected.replace(' ', "")));
    }

    #[test]
    fn crate_package_metadata_with_license_ids_and_file() {
        let mut context = Context::default();
        let crate_id = CrateId::new("mock_crate".to_owned(), VERSION_ZERO_ONE_ZERO);
        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: Some("http://www.mock_crate.com/".to_owned()),
                license_ids: BTreeSet::from(["Apache-2.0".to_owned(), "MIT".to_owned()]),
                license_file: Some("LICENSE.txt".to_owned()),
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                repository: None,
                license: None,
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let mut render_config = mock_render_config(None);
        Arc::get_mut(&mut render_config)
            .unwrap()
            .generate_rules_license_metadata = true;
        let renderer = Renderer::new(render_config, mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();

        let build_file_content = output
            .get(&PathBuf::from("BUILD.mock_crate-0.1.0.bazel"))
            .unwrap();

        let expected = indoc! {r#"
            package(
                default_package_metadata = [
                    ":license",
                    ":package_info",
                ],
                default_visibility = ["//visibility:public"],
            )

            package_info(
                name = "package_info",
                package_name = "mock_crate",
                package_version = "0.1.0",
                package_url = "http://www.mock_crate.com/",
            )

            license(
                name = "license",
                license_kinds = [
                    "@rules_license//licenses/spdx:Apache-2.0",
                    "@rules_license//licenses/spdx:MIT",
                ],
                license_text = "LICENSE.txt",
            )
        "#};
        assert!(build_file_content
            .replace(' ', "")
            .contains(&expected.replace(' ', "")));
    }

    #[test]
    fn write_outputs_semver_metadata() {
        let mut context = Context::default();
        // generate crate for libbpf-sys-1.4.0-v1.4.0
        let mut version = semver::Version::new(1, 4, 0);
        version.build = semver::BuildMetadata::new("v1.4.0").unwrap();
        // ensure metadata has a +
        assert!(version.to_string().contains('+'));
        let crate_id = CrateId::new("libbpf-sys".to_owned(), version);

        context.crates.insert(
            crate_id.clone(),
            CrateContext {
                name: crate_id.name,
                version: crate_id.version,
                package_url: None,
                repository: None,
                targets: BTreeSet::from([Rule::Library(mock_target_attributes())]),
                library_target_name: None,
                common_attrs: CommonAttributes::default(),
                build_script_attrs: None,
                license: None,
                license_ids: BTreeSet::default(),
                license_file: None,
                additive_build_file_content: None,
                disable_pipelining: false,
                extra_aliased_targets: BTreeMap::default(),
                alias_rule: None,
                override_targets: BTreeMap::default(),
            },
        );

        let mut config = mock_render_config(Some(VendorMode::Local));
        // change templates so it matches local vendor
        Arc::get_mut(&mut config).unwrap().build_file_template =
            "//{name}-{version}:BUILD.bazel".into();

        // Enable local vendor mode
        let renderer = Renderer::new(config, mock_supported_platform_triples());
        let output = renderer.render(&context, None).unwrap();
        eprintln!("output before {:?}", output.keys());
        // Local vendoring does not produce a `crate_repositories` macro
        let defs_module = output.get(&PathBuf::from("defs.bzl")).unwrap();
        assert!(!defs_module.contains("def crate_repositories():"));

        // Local vendoring does not produce a `crates.bzl` file.
        assert!(!output.contains_key(&PathBuf::from("crates.bzl")));

        // create tempdir to write to
        let outdir = tempfile::tempdir().unwrap();

        // create dir to mimic cargo vendor
        let _ = std::fs::create_dir_all(outdir.path().join("libbpf-sys-1.4.0+v1.4.0"));

        let normalized_outputs = normalize_cargo_file_paths(output, outdir.path());
        eprintln!(
            "Normalized outputs are {:?}",
            normalized_outputs.clone().into_keys()
        );

        write_outputs(normalized_outputs, false).unwrap();
        let expected = outdir.path().join("libbpf-sys-1.4.0-v1.4.0");
        let mut found = false;
        // ensure no files paths have a + sign
        for entry in fs::read_dir(outdir.path()).unwrap() {
            let path_str = entry.as_ref().unwrap().path().to_str().unwrap().to_string();
            if entry.unwrap().path() == expected {
                found = true;
            }
            assert!(!path_str.contains('+'));
        }
        assert!(found);
    }
}
