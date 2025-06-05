use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Debug;

use serde::ser::{SerializeMap, Serializer};
use serde::Serialize;
use serde_starlark::{FunctionCall, MULTILINE};

use crate::select::{Select, SelectableScalar};
use crate::utils::starlark::{
    looks_like_bazel_configuration_label, NoMatchingPlatformTriples, WithOriginalConfigurations,
};

#[derive(Debug, PartialEq, Eq)]
pub(crate) struct SelectScalar<T>
where
    T: SelectableScalar,
{
    common: Option<T>,
    selects: BTreeMap<String, WithOriginalConfigurations<T>>,
    // Elements from the `Select` whose configuration did not get mapped to any
    // new configuration. They could be ignored, but are preserved here to
    // generate comments that help the user understand what happened.
    unmapped: BTreeMap<String, T>,
}

impl<T> SelectScalar<T>
where
    T: SelectableScalar,
{
    /// Re-keys the provided Select by the given configuration mapping.
    /// This mapping maps from configurations in the input Select to sets of
    /// configurations in the output SelectScalar.
    pub(crate) fn new(select: Select<T>, platforms: &BTreeMap<String, BTreeSet<String>>) -> Self {
        let (common, selects) = select.into_parts();

        // Map new configuration -> WithOriginalConfigurations(value, old configurations).
        let mut remapped: BTreeMap<String, WithOriginalConfigurations<T>> = BTreeMap::new();
        // Map unknown configuration -> value.
        let mut unmapped: BTreeMap<String, T> = BTreeMap::new();

        for (original_configuration, value) in selects {
            match platforms.get(&original_configuration) {
                Some(configurations) => {
                    for configuration in configurations {
                        remapped
                            .entry(configuration.clone())
                            .or_insert_with(|| WithOriginalConfigurations {
                                value: value.clone(),
                                original_configurations: BTreeSet::new(),
                            })
                            .original_configurations
                            .insert(original_configuration.clone());
                    }
                }
                None => {
                    if looks_like_bazel_configuration_label(&original_configuration) {
                        remapped
                            .entry(original_configuration.clone())
                            .or_insert_with(|| WithOriginalConfigurations {
                                value: value.clone(),
                                original_configurations: BTreeSet::new(),
                            })
                            .original_configurations
                            .insert(original_configuration.clone());
                    } else {
                        unmapped.insert(original_configuration.clone(), value);
                    }
                }
            }
        }

        Self {
            common,
            selects: remapped,
            unmapped,
        }
    }

    /// Determine whether or not the select should be serialized
    pub(crate) fn is_empty(&self) -> bool {
        self.common.is_none() && self.selects.is_empty() && self.unmapped.is_empty()
    }
}

impl<T> Serialize for SelectScalar<T>
where
    T: SelectableScalar,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // If there are no platform-specific entries, we output just an ordinary
        // value.
        //
        // If there are platform-specific ones, we use the following.
        //
        //     select({
        //         "configuration": "plat-value",  # cfg(whatever),
        //         "//conditions:default": "common-value",
        //     })
        //
        // If there are unmapped entries, we include them like this:
        //
        //     selects.with_unmapped({
        //         "configuration": "plat-value",  # cfg(whatever),
        //         "//conditions:default": "common-value",
        //         selects.NO_MATCHING_PLATFORM_TRIPLES: {
        //             "cfg(obscure)": [
        //                 "unmapped-value",
        //             ],
        //         },
        //     })

        if self.common.is_some() && self.selects.is_empty() && self.unmapped.is_empty() {
            return self.common.as_ref().unwrap().serialize(serializer);
        }

        struct SelectInner<'a, T>(&'a SelectScalar<T>)
        where
            T: SelectableScalar;

        impl<T> Serialize for SelectInner<'_, T>
        where
            T: SelectableScalar,
        {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                let mut map = serializer.serialize_map(Some(MULTILINE))?;
                for (configuration, value) in self.0.selects.iter() {
                    map.serialize_entry(configuration, value)?;
                }
                if let Some(common) = self.0.common.as_ref() {
                    map.serialize_entry("//conditions:default", common)?;
                }
                if !self.0.unmapped.is_empty() {
                    struct SelectUnmapped<'a, T>(&'a BTreeMap<String, T>)
                    where
                        T: SelectableScalar;

                    impl<T> Serialize for SelectUnmapped<'_, T>
                    where
                        T: SelectableScalar,
                    {
                        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
                        where
                            S: Serializer,
                        {
                            let mut map = serializer.serialize_map(Some(MULTILINE))?;
                            for (cfg, value) in self.0.iter() {
                                map.serialize_entry(cfg, value)?;
                            }
                            map.end()
                        }
                    }

                    map.serialize_entry(
                        &NoMatchingPlatformTriples,
                        &SelectUnmapped(&self.0.unmapped),
                    )?;
                }
                map.end()
            }
        }

        let function = if self.unmapped.is_empty() {
            "select"
        } else {
            "selects.with_unmapped"
        };

        FunctionCall::new(function, [SelectInner(self)]).serialize(serializer)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use indoc::indoc;

    #[test]
    fn empty_select_value() {
        let select_value: SelectScalar<String> =
            SelectScalar::new(Default::default(), &Default::default());

        let expected_starlark = indoc! {r#"
            select({})
        "#};

        assert_eq!(
            select_value.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn no_platform_specific_select_value() {
        let mut select: Select<String> = Select::default();
        select.insert("Hello".to_owned(), None);

        let select_value = SelectScalar::new(select, &Default::default());

        let expected_starlark = indoc! {r#"
            "Hello"
        "#};

        assert_eq!(
            select_value.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn only_platform_specific_select_value() {
        let mut select: Select<String> = Select::default();
        select.insert("Hello".to_owned(), Some("platform".to_owned()));

        let platforms = BTreeMap::from([(
            "platform".to_owned(),
            BTreeSet::from(["platform".to_owned()]),
        )]);

        let select_value = SelectScalar::new(select, &platforms);

        let expected_starlark = indoc! {r#"
            select({
                "platform": "Hello",  # platform
            })
        "#};

        assert_eq!(
            select_value.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn mixed_select_value() {
        let mut select: Select<String> = Select::default();
        select.insert("Hello".to_owned(), Some("platform".to_owned()));
        select.insert("Goodbye".to_owned(), None);

        let platforms = BTreeMap::from([(
            "platform".to_owned(),
            BTreeSet::from(["platform".to_owned()]),
        )]);

        let select_value = SelectScalar::new(select, &platforms);

        let expected_starlark = indoc! {r#"
            select({
                "platform": "Hello",  # platform
                "//conditions:default": "Goodbye",
            })
        "#};

        assert_eq!(
            select_value.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn remap_select_value_configurations() {
        let mut select: Select<String> = Select::default();
        select.insert("a".to_owned(), Some("cfg(macos)".to_owned()));
        select.insert("a".to_owned(), Some("cfg(x86_64)".to_owned()));
        select.insert("e".to_owned(), Some("cfg(pdp11)".to_owned()));
        select.insert("f".to_owned(), Some("@platforms//os:magic".to_owned()));
        select.insert("g".to_owned(), Some("//another:platform".to_owned()));

        let platforms = BTreeMap::from([
            (
                "cfg(macos)".to_owned(),
                BTreeSet::from(["x86_64-macos".to_owned(), "aarch64-macos".to_owned()]),
            ),
            (
                "cfg(x86_64)".to_owned(),
                BTreeSet::from(["x86_64-linux".to_owned(), "x86_64-macos".to_owned()]),
            ),
        ]);

        let select_value = SelectScalar::new(select, &platforms);

        let expected = SelectScalar {
            common: None,
            selects: BTreeMap::from([
                (
                    "x86_64-macos".to_owned(),
                    WithOriginalConfigurations {
                        value: "a".to_owned(),
                        original_configurations: BTreeSet::from([
                            "cfg(macos)".to_owned(),
                            "cfg(x86_64)".to_owned(),
                        ]),
                    },
                ),
                (
                    "aarch64-macos".to_owned(),
                    WithOriginalConfigurations {
                        value: "a".to_owned(),
                        original_configurations: BTreeSet::from(["cfg(macos)".to_owned()]),
                    },
                ),
                (
                    "x86_64-linux".to_owned(),
                    WithOriginalConfigurations {
                        value: "a".to_owned(),
                        original_configurations: BTreeSet::from(["cfg(x86_64)".to_owned()]),
                    },
                ),
                (
                    "@platforms//os:magic".to_owned(),
                    WithOriginalConfigurations {
                        value: "f".to_owned(),
                        original_configurations: BTreeSet::from(
                            ["@platforms//os:magic".to_owned()],
                        ),
                    },
                ),
                (
                    "//another:platform".to_owned(),
                    WithOriginalConfigurations {
                        value: "g".to_owned(),
                        original_configurations: BTreeSet::from(["//another:platform".to_owned()]),
                    },
                ),
            ]),
            unmapped: BTreeMap::from([("cfg(pdp11)".to_owned(), "e".to_owned())]),
        };

        assert_eq!(select_value, expected);

        let expected_starlark = indoc! {r#"
            selects.with_unmapped({
                "//another:platform": "g",  # //another:platform
                "@platforms//os:magic": "f",  # @platforms//os:magic
                "aarch64-macos": "a",  # cfg(macos)
                "x86_64-linux": "a",  # cfg(x86_64)
                "x86_64-macos": "a",  # cfg(macos), cfg(x86_64)
                selects.NO_MATCHING_PLATFORM_TRIPLES: {
                    "cfg(pdp11)": "e",
                },
            })
        "#};

        assert_eq!(
            select_value.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }
}
