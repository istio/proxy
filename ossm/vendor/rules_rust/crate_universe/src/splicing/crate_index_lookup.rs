use crate::splicing::SourceInfo;
use anyhow::{Context, Result};
use crates_index::IndexConfig;
use hex::ToHex;

pub(crate) enum CrateIndexLookup {
    Git(crates_index::GitIndex),
    Http(crates_index::SparseIndex),
}

impl CrateIndexLookup {
    pub(crate) fn get_source_info(&self, pkg: &cargo_lock::Package) -> Result<Option<SourceInfo>> {
        let index_config = self
            .index_config()
            .context("Failed to get crate index config")?;
        let crate_ = match self {
            // The crates we care about should all be in the cache already,
            // because `cargo metadata` ran which should have fetched them.
            Self::Http(index) => Some(
                index
                    .crate_from_cache(pkg.name.as_str())
                    .with_context(|| format!("Failed to get crate from cache for {pkg:?}"))?,
            ),
            Self::Git(index) => index.crate_(pkg.name.as_str()),
        };
        let source_info = crate_.and_then(|crate_idx| {
            crate_idx
                .versions()
                .iter()
                .find(|v| v.version() == pkg.version.to_string())
                .and_then(|v| {
                    v.download_url(&index_config).map(|url| {
                        let sha256 = pkg
                            .checksum
                            .as_ref()
                            .and_then(|sum| sum.as_sha256().map(|sum| sum.encode_hex::<String>()))
                            .unwrap_or_else(|| v.checksum().encode_hex::<String>());
                        SourceInfo { url, sha256 }
                    })
                })
        });
        Ok(source_info)
    }

    #[allow(clippy::result_large_err)]
    fn index_config(&self) -> Result<IndexConfig, crates_index::Error> {
        match self {
            Self::Git(index) => index.index_config(),
            Self::Http(index) => index.index_config(),
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

            let index = CrateIndexLookup::Http(
                crates_index::SparseIndex::from_url("sparse+https://index.crates.io/").unwrap(),
            );
            let source_info = index
                .get_source_info(&cargo_lock::Package {
                    name: "lazy_static".parse().unwrap(),
                    version: Version::parse("1.4.0").unwrap(),
                    source: None,
                    checksum: None,
                    dependencies: Vec::new(),
                    replace: None,
                })
                .unwrap()
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

            let index = CrateIndexLookup::Http(
                crates_index::SparseIndex::from_url("sparse+https://index.crates.io/").unwrap(),
            );
            let source_info = index
                .get_source_info(&cargo_lock::Package {
                    name: "lazy_static".parse().unwrap(),
                    version: Version::parse("1.4.0").unwrap(),
                    source: None,
                    checksum: None,
                    dependencies: Vec::new(),
                    replace: None,
                })
                .unwrap()
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
