use std::collections::BTreeSet;

use serde::ser::{SerializeMap, SerializeTupleStruct, Serializer};
use serde::Serialize;
use serde_starlark::{FunctionCall, MULTILINE};

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub(crate) struct TargetCompatibleWith {
    target_triples: BTreeSet<String>,
}

impl TargetCompatibleWith {
    pub(crate) fn new(target_triples: BTreeSet<String>) -> Self {
        TargetCompatibleWith { target_triples }
    }
}

impl Serialize for TargetCompatibleWith {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // Output looks like:
        //
        //     select({
        //         "configuration": [],
        //         "//conditions:default": ["@platforms//:incompatible"],
        //     })

        let mut plus = serializer.serialize_tuple_struct("+", MULTILINE)?;

        struct SelectInner<'a>(&'a BTreeSet<String>);

        impl Serialize for SelectInner<'_> {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                let mut map = serializer.serialize_map(Some(MULTILINE))?;
                for cfg in self.0 {
                    map.serialize_entry(cfg, &[] as &[String])?;
                }
                map.serialize_entry(
                    "//conditions:default",
                    &["@platforms//:incompatible".to_owned()] as &[String],
                )?;
                map.end()
            }
        }

        plus.serialize_field(&FunctionCall::new(
            "select",
            [SelectInner(&self.target_triples)],
        ))?;

        plus.end()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use indoc::indoc;

    #[test]
    fn target_compatible_with() {
        let target_compatible_with = TargetCompatibleWith::new(BTreeSet::from([
            "@rules_rust//rust/platform:wasm32-unknown-unknown".to_owned(),
            "@rules_rust//rust/platform:wasm32-wasip1".to_owned(),
            "@rules_rust//rust/platform:x86_64-apple-darwin".to_owned(),
            "@rules_rust//rust/platform:x86_64-pc-windows-msvc".to_owned(),
            "@rules_rust//rust/platform:x86_64-unknown-linux-gnu".to_owned(),
        ]));

        let expected_starlark = indoc! {r#"
            select({
                "@rules_rust//rust/platform:wasm32-unknown-unknown": [],
                "@rules_rust//rust/platform:wasm32-wasip1": [],
                "@rules_rust//rust/platform:x86_64-apple-darwin": [],
                "@rules_rust//rust/platform:x86_64-pc-windows-msvc": [],
                "@rules_rust//rust/platform:x86_64-unknown-linux-gnu": [],
                "//conditions:default": ["@platforms//:incompatible"],
            })
        "#};

        assert_eq!(
            target_compatible_with
                .serialize(serde_starlark::Serializer)
                .unwrap(),
            expected_starlark,
        );
    }
}
