use std::fmt::{Display, Formatter, Result};

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(transparent)]
pub(crate) struct TargetTriple(String);

impl TargetTriple {
    #[cfg(test)]
    pub(crate) fn from_bazel(bazel: String) -> Self {
        Self(bazel)
    }

    pub(crate) fn to_bazel(&self) -> String {
        self.0.clone()
    }

    pub(crate) fn to_cargo(&self) -> String {
        // While Bazel is NixOS aware (via `@platforms//os:nixos`), `rustc`
        // is not, so any target triples for `nixos` get remapped to `linux`
        // for the purposes of determining `cargo metadata`, resolving `cfg`
        // targets, etc.
        self.0.replace("nixos", "linux")
    }
}

impl Display for TargetTriple {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result {
        let bazel = self.to_bazel();
        let cargo = self.to_cargo();
        match bazel == cargo {
            true => write!(f, "{}", bazel),
            false => write!(f, "{} (cargo: {})", bazel, cargo),
        }
    }
}
