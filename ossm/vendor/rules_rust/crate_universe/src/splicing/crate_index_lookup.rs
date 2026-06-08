use crate::splicing::SourceInfo;
use anyhow::{Context, Result};
use crates_index::IndexConfig;
use hex::ToHex;

pub(crate) enum CrateIndexLookup {
    Git {
        index: crates_index::GitIndex,
        index_config: IndexConfig,
    },
    Http {
        index: crates_index::SparseIndex,
        index_config: IndexConfig,
    },
}

impl CrateIndexLookup {
    pub(crate) fn get_source_info(&self, pkg: &cargo_lock::Package) -> Result<SourceInfo> {
        let url = self
            .index_config()
            .download_url(pkg.name.as_str(), &pkg.version.to_string())
            .context("no url for crate")?;
        let sha256 = pkg
            .checksum
            .as_ref()
            .and_then(|sum| sum.as_sha256().map(|sum| sum.encode_hex::<String>()))
            .unwrap_or_else(|| {
                let crate_ = match self {
                    // The crates we care about should all be in the cache already,
                    // because `cargo metadata` ran which should have fetched them.
                    Self::Http { index, .. } => Some(
                        index
                            .crate_from_cache(pkg.name.as_str())
                            .with_context(|| {
                                format!("Failed to get crate from cache: {:?}\n{:?}", index, pkg)
                            })
                            .unwrap(),
                    ),
                    Self::Git { index, .. } => index.crate_(pkg.name.as_str()),
                };
                crate_
                    .and_then(|crate_idx| {
                        crate_idx
                            .versions()
                            .iter()
                            .find(|v| v.version() == pkg.version.to_string())
                            .map(|v| v.checksum().encode_hex::<String>())
                    })
                    .unwrap()
            });

        Ok(SourceInfo { url, sha256 })
    }

    #[allow(clippy::result_large_err)]
    fn index_config(&self) -> &IndexConfig {
        match self {
            Self::Git { index_config, .. } => index_config,
            Self::Http { index_config, .. } => index_config,
        }
    }
}

#[cfg(test)]
mod test {
    use crate::splicing::crate_index_lookup::CrateIndexLookup;
    use semver::Version;
    use std::ffi::OsString;

    // TODO: Avoid global state (env vars) in these tests.
    // TODO: These should be separate tests methods but they have conflicting state.

    #[test]
    fn sparse_index() {
        let runfiles = runfiles::Runfiles::create().unwrap();
        {
            let _e = EnvVarResetter::set(
                "CARGO_HOME",
                runfiles::rlocation!(
                    runfiles,
                    "rules_rust/crate_universe/test_data/crate_indexes/lazy_static/cargo_home"
                )
                .unwrap(),
            );

            let index =
                crates_index::SparseIndex::from_url("sparse+https://index.crates.io/").unwrap();
            let index_config = index.index_config().unwrap();
            let index = CrateIndexLookup::Http {
                index,
                index_config,
            };

            let source_info = index
                .get_source_info(&cargo_lock::Package {
                    name: "lazy_static".parse().unwrap(),
                    version: Version::parse("1.4.0").unwrap(),
                    source: None,
                    checksum: None,
                    dependencies: Vec::new(),
                    replace: None,
                })
                .unwrap();
            assert_eq!(
                source_info.url,
                "https://crates.io/api/v1/crates/lazy_static/1.4.0/download"
            );
            assert_eq!(
                source_info.sha256,
                "e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646"
            );
        }
        {
            let _e = EnvVarResetter::set("CARGO_HOME",
                runfiles::rlocation!(runfiles, "rules_rust/crate_universe/test_data/crate_indexes/rewritten_lazy_static/cargo_home").unwrap());

            let index =
                crates_index::SparseIndex::from_url("sparse+https://index.crates.io/").unwrap();
            let index_config = index.index_config().unwrap();
            let index = CrateIndexLookup::Http {
                index,
                index_config,
            };

            let source_info = index
                .get_source_info(&cargo_lock::Package {
                    name: "lazy_static".parse().unwrap(),
                    version: Version::parse("1.4.0").unwrap(),
                    source: None,
                    checksum: None,
                    dependencies: Vec::new(),
                    replace: None,
                })
                .unwrap();
            assert_eq!(
                source_info.url,
                "https://some-mirror.com/api/v1/crates/lazy_static/1.4.0/download"
            );
            assert_eq!(
                source_info.sha256,
                "fffffffffbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646"
            );
        }
    }

    struct EnvVarResetter {
        key: OsString,
        value: Option<OsString>,
    }

    impl EnvVarResetter {
        fn set<K: Into<OsString>, V: Into<OsString>>(key: K, value: V) -> EnvVarResetter {
            let key = key.into();
            let value = value.into();
            let old_value = std::env::var_os(&key);

            std::env::set_var(&key, value);

            EnvVarResetter {
                key,
                value: old_value,
            }
        }
    }

    impl Drop for EnvVarResetter {
        fn drop(&mut self) {
            if let Some(old_value) = self.value.as_ref() {
                std::env::set_var(&self.key, old_value);
            } else {
                std::env::remove_var(&self.key);
            }
        }
    }
}
