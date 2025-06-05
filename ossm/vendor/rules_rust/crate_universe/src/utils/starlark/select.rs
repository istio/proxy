use std::collections::BTreeSet;

use serde::ser::Serializer;
use serde::Serialize;
use serde_starlark::LineComment;

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub(crate) struct WithOriginalConfigurations<T> {
    pub(crate) value: T,
    pub(crate) original_configurations: BTreeSet<String>,
}

#[derive(Serialize)]
#[serde(rename = "selects.NO_MATCHING_PLATFORM_TRIPLES")]
pub(crate) struct NoMatchingPlatformTriples;

impl<T> Serialize for WithOriginalConfigurations<T>
where
    T: Serialize,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let comment =
            Vec::from_iter(self.original_configurations.iter().map(String::as_str)).join(", ");
        LineComment::new(&self.value, &comment).serialize(serializer)
    }
}

// We allow users to specify labels as keys to selects, but we need to identify when this is happening
// because we also allow things like "x86_64-unknown-linux-gnu" as keys, and these technically parse as labels
// (that parses as "//x86_64-unknown-linux-gnu:x86_64-unknown-linux-gnu").
//
// We don't expect any cfg-expressions or target triples to contain //,
// and all labels _can_ be written in a way that they contain //,
// so we use the presence of // as an indication something is a label.
pub(crate) fn looks_like_bazel_configuration_label(configuration: &str) -> bool {
    configuration.contains("//")
}
