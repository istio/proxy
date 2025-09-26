//! Tools for parsing [Cargo configuration](https://doc.rust-lang.org/cargo/reference/config.html) files

use std::collections::BTreeMap;
use std::fs;
use std::path::Path;
use std::str::FromStr;

use crate::utils;
use anyhow::{bail, Result};
use serde::{Deserialize, Serialize};

/// The [`[registry]`](https://doc.rust-lang.org/cargo/reference/config.html#registry)
/// table controls the default registry used when one is not specified.
#[derive(Debug, Deserialize, Serialize, PartialEq, Eq)]
pub(crate) struct Registry {
    /// name of the default registry
    pub(crate) default: String,

    /// authentication token for crates.io
    pub(crate) token: Option<String>,
}

/// The [`[source]`](https://doc.rust-lang.org/cargo/reference/config.html#source)
/// table defines the registry sources available.
#[derive(Debug, Deserialize, Serialize, PartialEq, Eq)]
pub(crate) struct Source {
    /// replace this source with the given named source
    #[serde(rename = "replace-with")]
    pub(crate) replace_with: Option<String>,

    /// URL to a registry source
    #[serde(default = "default_registry_url")]
    pub(crate) registry: String,
}

/// This is the default registry url per what's defined by Cargo.
fn default_registry_url() -> String {
    utils::CRATES_IO_INDEX_URL.to_owned()
}

#[derive(Debug, Deserialize, Serialize, PartialEq, Eq)]
/// registries other than crates.io
pub(crate) struct AdditionalRegistry {
    /// URL of the registry index
    pub(crate) index: String,

    /// authentication token for the registry
    pub(crate) token: Option<String>,
}

/// A subset of a Cargo configuration file. The schema here is only what
/// is required for parsing registry information.
/// See [cargo docs](https://doc.rust-lang.org/cargo/reference/config.html#configuration-format)
/// for more details.
#[derive(Debug, Deserialize, Serialize, PartialEq, Eq)]
pub(crate) struct CargoConfig {
    /// registries other than crates.io
    #[serde(default = "default_registries")]
    pub(crate) registries: BTreeMap<String, AdditionalRegistry>,

    #[serde(default = "default_registry")]
    pub(crate) registry: Registry,

    /// source definition and replacement
    #[serde(default = "BTreeMap::new")]
    pub(crate) source: BTreeMap<String, Source>,
}

/// Each Cargo config is expected to have a default `crates-io` registry.
fn default_registries() -> BTreeMap<String, AdditionalRegistry> {
    let mut registries = BTreeMap::new();
    registries.insert(
        "crates-io".to_owned(),
        AdditionalRegistry {
            index: default_registry_url(),
            token: None,
        },
    );
    registries
}

/// Each Cargo config has a default registry for `crates.io`.
fn default_registry() -> Registry {
    Registry {
        default: "crates-io".to_owned(),
        token: None,
    }
}

impl Default for CargoConfig {
    fn default() -> Self {
        let registries = default_registries();
        let registry = default_registry();
        let source = Default::default();

        Self {
            registries,
            registry,
            source,
        }
    }
}

impl FromStr for CargoConfig {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let incoming: CargoConfig = toml::from_str(s)?;
        let mut config = Self::default();
        config.registries.extend(incoming.registries);
        config.source.extend(incoming.source);
        config.registry = incoming.registry;
        Ok(config)
    }
}

impl CargoConfig {
    /// Load a Cargo config from a path to a file on disk.
    pub(crate) fn try_from_path(path: &Path) -> Result<Self> {
        let content = fs::read_to_string(path)?;
        Self::from_str(&content)
    }

    /// Look up a registry [Source] by its url.
    pub(crate) fn get_source_from_url(&self, url: &str) -> Option<&Source> {
        if let Some(found) = self.source.values().find(|v| v.registry == url) {
            Some(found)
        } else if url == utils::CRATES_IO_INDEX_URL {
            self.source.get("crates-io")
        } else {
            None
        }
    }

    pub(crate) fn get_registry_index_url_by_name(&self, name: &str) -> Option<&str> {
        if let Some(registry) = self.registries.get(name) {
            Some(&registry.index)
        } else if let Some(source) = self.source.get(name) {
            Some(&source.registry)
        } else {
            None
        }
    }

    pub(crate) fn resolve_replacement_url<'a>(&'a self, url: &'a str) -> Result<&'a str> {
        if let Some(source) = self.get_source_from_url(url) {
            if let Some(replace_with) = &source.replace_with {
                if let Some(replacement) = self.get_registry_index_url_by_name(replace_with) {
                    Ok(replacement)
                } else {
                    bail!("Tried to replace registry {} with registry named {} but didn't have metadata about the replacement", url, replace_with);
                }
            } else {
                Ok(url)
            }
        } else {
            Ok(url)
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn registry_settings() {
        let temp_dir = tempfile::tempdir().unwrap();
        let config = temp_dir.as_ref().join("config.toml");

        fs::write(&config, textwrap::dedent(
            r#"
                # Makes artifactory the default registry and saves passing --registry parameter
                [registry]
                default = "art-crates-remote"
                
                [registries]
                # Remote repository proxy in Artifactory (read-only)
                art-crates-remote = { index = "https://artprod.mycompany/artifactory/git/cargo-remote.git" }
                
                # Optional, use with --registry to publish to crates.io
                crates-io = { index = "https://github.com/rust-lang/crates.io-index" }

                [net]
                git-fetch-with-cli = true
            "#,
        )).unwrap();

        let config = CargoConfig::try_from_path(&config).unwrap();
        assert_eq!(
            config,
            CargoConfig {
                registries: BTreeMap::from([
                    (
                        "art-crates-remote".to_owned(),
                        AdditionalRegistry {
                            index: "https://artprod.mycompany/artifactory/git/cargo-remote.git"
                                .to_owned(),
                            token: None,
                        },
                    ),
                    (
                        "crates-io".to_owned(),
                        AdditionalRegistry {
                            index: "https://github.com/rust-lang/crates.io-index".to_owned(),
                            token: None,
                        },
                    ),
                ]),
                registry: Registry {
                    default: "art-crates-remote".to_owned(),
                    token: None,
                },
                source: BTreeMap::new(),
            },
        )
    }

    #[test]
    fn registry_settings_get_index_url_by_name_from_source() {
        let temp_dir = tempfile::tempdir().unwrap();
        let config = temp_dir.as_ref().join("config.toml");

        fs::write(&config, textwrap::dedent(
            r#"
                [registries]
                art-crates-remote = { index = "https://artprod.mycompany/artifactory/git/cargo-remote.git" }

                [source.crates-io]
                replace-with = "some-mirror"

                [source.some-mirror]
                registry = "https://artmirror.mycompany/artifactory/cargo-mirror.git"
            "#,
        )).unwrap();

        let config = CargoConfig::try_from_path(&config).unwrap();
        assert_eq!(
            config.get_registry_index_url_by_name("some-mirror"),
            Some("https://artmirror.mycompany/artifactory/cargo-mirror.git"),
        );
    }

    #[test]
    fn registry_settings_get_index_url_by_name_from_registry() {
        let temp_dir = tempfile::tempdir().unwrap();
        let config = temp_dir.as_ref().join("config.toml");

        fs::write(&config, textwrap::dedent(
            r#"
                [registries]
                art-crates-remote = { index = "https://artprod.mycompany/artifactory/git/cargo-remote.git" }

                [source.crates-io]
                replace-with = "art-crates-remote"
            "#,
        )).unwrap();

        let config = CargoConfig::try_from_path(&config).unwrap();
        assert_eq!(
            config.get_registry_index_url_by_name("art-crates-remote"),
            Some("https://artprod.mycompany/artifactory/git/cargo-remote.git"),
        );
    }

    #[test]
    fn registry_settings_get_source_from_url() {
        let temp_dir = tempfile::tempdir().unwrap();
        let config = temp_dir.as_ref().join("config.toml");

        fs::write(
            &config,
            textwrap::dedent(
                r#"
                [source.some-mirror]
                registry = "https://artmirror.mycompany/artifactory/cargo-mirror.git"
            "#,
            ),
        )
        .unwrap();

        let config = CargoConfig::try_from_path(&config).unwrap();
        assert_eq!(
            config
                .get_source_from_url("https://artmirror.mycompany/artifactory/cargo-mirror.git")
                .map(|s| s.registry.as_str()),
            Some("https://artmirror.mycompany/artifactory/cargo-mirror.git"),
        );
    }

    #[test]
    fn resolve_replacement_url_no_replacement() {
        let temp_dir = tempfile::tempdir().unwrap();
        let config = temp_dir.as_ref().join("config.toml");

        fs::write(&config, "").unwrap();

        let config = CargoConfig::try_from_path(&config).unwrap();

        assert_eq!(
            config
                .resolve_replacement_url(utils::CRATES_IO_INDEX_URL)
                .unwrap(),
            utils::CRATES_IO_INDEX_URL
        );
        assert_eq!(
            config
                .resolve_replacement_url("https://artmirror.mycompany/artifactory/cargo-mirror.git")
                .unwrap(),
            "https://artmirror.mycompany/artifactory/cargo-mirror.git"
        );
    }

    #[test]
    fn resolve_replacement_url_registry() {
        let temp_dir = tempfile::tempdir().unwrap();
        let config = temp_dir.as_ref().join("config.toml");

        fs::write(&config, textwrap::dedent(
            r#"
                [registries]
                art-crates-remote = { index = "https://artprod.mycompany/artifactory/git/cargo-remote.git" }

                [source.crates-io]
                replace-with = "some-mirror"

                [source.some-mirror]
                registry = "https://artmirror.mycompany/artifactory/cargo-mirror.git"
            "#,
        )).unwrap();

        let config = CargoConfig::try_from_path(&config).unwrap();
        assert_eq!(
            config
                .resolve_replacement_url(utils::CRATES_IO_INDEX_URL)
                .unwrap(),
            "https://artmirror.mycompany/artifactory/cargo-mirror.git"
        );
        assert_eq!(
            config
                .resolve_replacement_url("https://artmirror.mycompany/artifactory/cargo-mirror.git")
                .unwrap(),
            "https://artmirror.mycompany/artifactory/cargo-mirror.git"
        );
        assert_eq!(
            config
                .resolve_replacement_url(
                    "https://artprod.mycompany/artifactory/git/cargo-remote.git"
                )
                .unwrap(),
            "https://artprod.mycompany/artifactory/git/cargo-remote.git"
        );
    }

    #[test]
    fn resolve_replacement_url_source() {
        let temp_dir = tempfile::tempdir().unwrap();
        let config = temp_dir.as_ref().join("config.toml");

        fs::write(&config, textwrap::dedent(
            r#"
                [registries]
                art-crates-remote = { index = "https://artprod.mycompany/artifactory/git/cargo-remote.git" }

                [source.crates-io]
                replace-with = "art-crates-remote"

                [source.some-mirror]
                registry = "https://artmirror.mycompany/artifactory/cargo-mirror.git"
            "#,
        )).unwrap();

        let config = CargoConfig::try_from_path(&config).unwrap();
        assert_eq!(
            config
                .resolve_replacement_url(utils::CRATES_IO_INDEX_URL)
                .unwrap(),
            "https://artprod.mycompany/artifactory/git/cargo-remote.git"
        );
        assert_eq!(
            config
                .resolve_replacement_url("https://artmirror.mycompany/artifactory/cargo-mirror.git")
                .unwrap(),
            "https://artmirror.mycompany/artifactory/cargo-mirror.git"
        );
        assert_eq!(
            config
                .resolve_replacement_url(
                    "https://artprod.mycompany/artifactory/git/cargo-remote.git"
                )
                .unwrap(),
            "https://artprod.mycompany/artifactory/git/cargo-remote.git"
        );
    }
}
