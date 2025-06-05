use std::collections::BTreeSet;
use std::fmt;

use serde::de::value::{MapAccessDeserializer, SeqAccessDeserializer};
use serde::de::{Deserialize, Deserializer, MapAccess, SeqAccess, Visitor};
use serde::ser::{Serialize, SerializeStruct, Serializer};

#[derive(Debug, Default, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub(crate) struct Glob {
    pub(crate) allow_empty: bool,
    pub(crate) include: BTreeSet<String>,
    pub(crate) exclude: BTreeSet<String>,
}

impl Glob {
    pub(crate) fn new_rust_srcs(allow_empty: bool) -> Self {
        Self {
            allow_empty,
            include: BTreeSet::from(["**/*.rs".to_owned()]),
            exclude: BTreeSet::new(),
        }
    }

    pub(crate) fn has_any_include(&self) -> bool {
        self.include.is_empty()
        // Note: self.exclude intentionally not considered. A glob is empty if
        // there are no included globs. A glob cannot have only excludes.
    }
}

impl Serialize for Glob {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let has_exclude = !self.exclude.is_empty();
        let len = 2 + if has_exclude { 1 } else { 0 };

        // Serialize as glob(allow_empty = False, include = [...], exclude = [...]).
        let mut call = serializer.serialize_struct("glob", len)?;
        call.serialize_field("allow_empty", &self.allow_empty)?;
        call.serialize_field("include", &self.include)?;
        if has_exclude {
            call.serialize_field("exclude", &self.exclude)?;
        }
        call.end()
    }
}

struct GlobVisitor;

impl<'de> Deserialize<'de> for Glob {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(GlobVisitor)
    }
}

impl<'de> Visitor<'de> for GlobVisitor {
    type Value = Glob;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("glob")
    }

    // Deserialize ["included","globs","only"]
    fn visit_seq<A>(self, seq: A) -> Result<Self::Value, A::Error>
    where
        A: SeqAccess<'de>,
    {
        Ok(Glob {
            // At time of writing the default value of allow_empty is true.
            // We may want to change this if the default changes in Bazel.
            allow_empty: true,
            include: BTreeSet::deserialize(SeqAccessDeserializer::new(seq))?,
            exclude: BTreeSet::new(),
        })
    }

    // Deserialize {"include":["included","globs"],"exclude":["excluded","globs"]}
    fn visit_map<A>(self, map: A) -> Result<Self::Value, A::Error>
    where
        A: MapAccess<'de>,
    {
        fn default_true() -> bool {
            true
        }

        #[derive(serde::Deserialize)]
        struct GlobMap {
            #[serde(default = "default_true")]
            allow_empty: bool,
            include: BTreeSet<String>,
            #[serde(default)]
            exclude: BTreeSet<String>,
        }

        let glob_map = GlobMap::deserialize(MapAccessDeserializer::new(map))?;
        Ok(Glob {
            allow_empty: glob_map.allow_empty,
            include: glob_map.include,
            exclude: glob_map.exclude,
        })
    }
}
