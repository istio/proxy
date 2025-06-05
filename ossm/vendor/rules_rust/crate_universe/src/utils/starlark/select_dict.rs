use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Debug;

use serde::ser::{SerializeMap, Serializer};
use serde::Serialize;
use serde_starlark::{FunctionCall, MULTILINE};

use crate::select::{Select, SelectableOrderedValue, SelectableValue};
use crate::utils::starlark::{
    looks_like_bazel_configuration_label, NoMatchingPlatformTriples, WithOriginalConfigurations,
};

#[derive(Debug, PartialEq, Eq)]
pub(crate) struct SelectDict<U, T>
where
    U: SelectableOrderedValue,
    T: SelectableValue,
{
    // Invariant: keys in this map are not in any of the inner maps of `selects`.
    common: BTreeMap<U, T>,
    // Invariant: none of the inner maps are empty.
    selects: BTreeMap<String, BTreeMap<U, WithOriginalConfigurations<T>>>,
    // Elements from the `Select` whose configuration did not get mapped to any
    // new configuration. They could be ignored, but are preserved here to
    // generate comments that help the user understand what happened.
    unmapped: BTreeMap<String, BTreeMap<U, T>>,
}

impl<U, T> SelectDict<U, T>
where
    U: SelectableOrderedValue,
    T: SelectableValue,
{
    /// Re-keys the provided Select by the given configuration mapping.
    /// This mapping maps from configurations in the input Select to sets
    /// of configurations in the output SelectDict.
    pub(crate) fn new(
        select: Select<BTreeMap<U, T>>,
        platforms: &BTreeMap<String, BTreeSet<String>>,
    ) -> Self {
        let (common, selects) = select.into_parts();

        // Map new configuration -> WithOriginalConfigurations(value, old configurations).
        let mut remapped: BTreeMap<String, BTreeMap<U, WithOriginalConfigurations<T>>> =
            BTreeMap::new();
        // Map unknown configuration -> value.
        let mut unmapped: BTreeMap<String, BTreeMap<U, T>> = BTreeMap::new();

        for (original_configuration, entries) in selects {
            match platforms.get(&original_configuration) {
                Some(configurations) => {
                    for configuration in configurations {
                        for (key, value) in &entries {
                            remapped
                                .entry(configuration.clone())
                                .or_default()
                                .entry(key.clone())
                                .or_insert_with(|| WithOriginalConfigurations {
                                    value: value.clone(),
                                    original_configurations: BTreeSet::new(),
                                })
                                .original_configurations
                                .insert(original_configuration.clone());
                        }
                    }
                }
                None => {
                    for (key, value) in entries {
                        if looks_like_bazel_configuration_label(&original_configuration) {
                            remapped
                                .entry(original_configuration.clone())
                                .or_default()
                                .entry(key)
                                .or_insert_with(|| WithOriginalConfigurations {
                                    value: value.clone(),
                                    original_configurations: BTreeSet::new(),
                                })
                                .original_configurations
                                .insert(original_configuration.clone());
                        } else {
                            unmapped
                                .entry(original_configuration.clone())
                                .or_default()
                                .insert(key, value);
                        };
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

    pub(crate) fn is_empty(&self) -> bool {
        self.common.is_empty() && self.selects.is_empty() && self.unmapped.is_empty()
    }
}

impl<U, T> Serialize for SelectDict<U, T>
where
    U: SelectableOrderedValue,
    T: SelectableValue,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // If there are no platform-specific entries, we output just an ordinary
        // dict.
        //
        // If there are platform-specific ones, we use the following. Ideally it
        // could be done as `dicts.add({...}, select({...}))` but bazel_skylib's
        // dicts.add does not support selects.
        //
        //     select({
        //         "configuration": {
        //             "common-key": "common-value",
        //             "plat-key": "plat-value",  # cfg(whatever)
        //         },
        //         "//conditions:default": {
        //             "common-key": "common-value",
        //         },
        //     })
        //
        // If there are unmapped entries, we include them like this:
        //
        //     selects.with_unmapped({
        //         "configuration": {
        //             "common-key": "common-value",
        //             "plat-key": "plat-value",  # cfg(whatever)
        //         },
        //         "//conditions:default": {
        //             "common-key": "common-value",
        //         },
        //         selects.NO_MATCHING_PLATFORM_TRIPLES: {
        //             "cfg(obscure): {
        //                 "unmapped-key": "unmapped-value",
        //             },
        //         },
        //     })

        if self.selects.is_empty() && self.unmapped.is_empty() {
            return self.common.serialize(serializer);
        }

        struct SelectInner<'a, U, T>(&'a SelectDict<U, T>)
        where
            U: SelectableOrderedValue,
            T: SelectableValue;

        impl<U, T> Serialize for SelectInner<'_, U, T>
        where
            U: SelectableOrderedValue,
            T: SelectableValue,
        {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                let mut map = serializer.serialize_map(Some(MULTILINE))?;
                for (configuration, dict) in &self.0.selects {
                    #[derive(Serialize)]
                    #[serde(untagged)]
                    enum Either<'a, T> {
                        Common(&'a T),
                        Selects(&'a WithOriginalConfigurations<T>),
                    }

                    let mut combined = BTreeMap::new();
                    combined.extend(
                        self.0
                            .common
                            .iter()
                            .map(|(key, value)| (key, Either::Common(value))),
                    );
                    combined.extend(
                        dict.iter()
                            .map(|(key, value)| (key, Either::Selects(value))),
                    );
                    map.serialize_entry(configuration, &combined)?;
                }
                map.serialize_entry("//conditions:default", &self.0.common)?;
                if !self.0.unmapped.is_empty() {
                    struct SelectUnmapped<'a, U, T>(&'a BTreeMap<String, BTreeMap<U, T>>)
                    where
                        U: SelectableOrderedValue,
                        T: SelectableValue;

                    impl<U, T> Serialize for SelectUnmapped<'_, U, T>
                    where
                        U: SelectableOrderedValue,
                        T: SelectableValue,
                    {
                        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
                        where
                            S: Serializer,
                        {
                            let mut map = serializer.serialize_map(Some(MULTILINE))?;
                            for (cfg, dict) in self.0.iter() {
                                map.serialize_entry(cfg, dict)?;
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
    fn empty_select_dict() {
        let select_dict: SelectDict<String, String> =
            SelectDict::new(Default::default(), &Default::default());

        let expected_starlark = indoc! {r#"
            {}
        "#};

        assert_eq!(
            select_dict.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn no_platform_specific_select_dict() {
        let mut select: Select<BTreeMap<String, String>> = Select::default();
        select.insert(("Greeting".to_owned(), "Hello".to_owned()), None);

        let select_dict = SelectDict::new(select, &Default::default());

        let expected_starlark = indoc! {r#"
            {
                "Greeting": "Hello",
            }
        "#};

        assert_eq!(
            select_dict.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn only_platform_specific_select_dict() {
        let mut select: Select<BTreeMap<String, String>> = Select::default();
        select.insert(
            ("Greeting".to_owned(), "Hello".to_owned()),
            Some("platform".to_owned()),
        );

        let platforms = BTreeMap::from([(
            "platform".to_owned(),
            BTreeSet::from(["platform".to_owned()]),
        )]);

        let select_dict = SelectDict::new(select, &platforms);

        let expected_starlark = indoc! {r#"
            select({
                "platform": {
                    "Greeting": "Hello",  # platform
                },
                "//conditions:default": {},
            })
        "#};

        assert_eq!(
            select_dict.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn mixed_select_dict() {
        let mut select: Select<BTreeMap<String, String>> = Select::default();
        select.insert(
            ("Greeting".to_owned(), "Hello".to_owned()),
            Some("platform".to_owned()),
        );
        select.insert(("Message".to_owned(), "Goodbye".to_owned()), None);

        let platforms = BTreeMap::from([(
            "platform".to_owned(),
            BTreeSet::from(["platform".to_owned()]),
        )]);

        let select_dict = SelectDict::new(select, &platforms);

        let expected_starlark = indoc! {r#"
            select({
                "platform": {
                    "Greeting": "Hello",  # platform
                    "Message": "Goodbye",
                },
                "//conditions:default": {
                    "Message": "Goodbye",
                },
            })
        "#};

        assert_eq!(
            select_dict.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn remap_select_dict_configurations() {
        let mut select: Select<BTreeMap<String, String>> = Select::default();
        select.insert(
            ("dep-a".to_owned(), "a".to_owned()),
            Some("cfg(macos)".to_owned()),
        );
        select.insert(
            ("dep-b".to_owned(), "b".to_owned()),
            Some("cfg(macos)".to_owned()),
        );
        select.insert(
            ("dep-d".to_owned(), "d".to_owned()),
            Some("cfg(macos)".to_owned()),
        );
        select.insert(
            ("dep-a".to_owned(), "a".to_owned()),
            Some("cfg(x86_64)".to_owned()),
        );
        select.insert(
            ("dep-c".to_owned(), "c".to_owned()),
            Some("cfg(x86_64)".to_owned()),
        );
        select.insert(
            ("dep-e".to_owned(), "e".to_owned()),
            Some("cfg(pdp11)".to_owned()),
        );
        select.insert(("dep-d".to_owned(), "d".to_owned()), None);
        select.insert(
            ("dep-f".to_owned(), "f".to_owned()),
            Some("@platforms//os:magic".to_owned()),
        );
        select.insert(
            ("dep-g".to_owned(), "g".to_owned()),
            Some("//another:platform".to_owned()),
        );

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

        let select_dict = SelectDict::new(select, &platforms);

        let expected = SelectDict {
            common: BTreeMap::from([("dep-d".to_string(), "d".to_owned())]),
            selects: BTreeMap::from([
                (
                    "x86_64-macos".to_owned(),
                    BTreeMap::from([
                        (
                            "dep-a".to_string(),
                            WithOriginalConfigurations {
                                value: "a".to_owned(),
                                original_configurations: BTreeSet::from([
                                    "cfg(macos)".to_owned(),
                                    "cfg(x86_64)".to_owned(),
                                ]),
                            },
                        ),
                        (
                            "dep-b".to_string(),
                            WithOriginalConfigurations {
                                value: "b".to_owned(),
                                original_configurations: BTreeSet::from(["cfg(macos)".to_owned()]),
                            },
                        ),
                        (
                            "dep-c".to_string(),
                            WithOriginalConfigurations {
                                value: "c".to_owned(),
                                original_configurations: BTreeSet::from(["cfg(x86_64)".to_owned()]),
                            },
                        ),
                    ]),
                ),
                (
                    "aarch64-macos".to_owned(),
                    BTreeMap::from([
                        (
                            "dep-a".to_string(),
                            WithOriginalConfigurations {
                                value: "a".to_owned(),
                                original_configurations: BTreeSet::from(["cfg(macos)".to_owned()]),
                            },
                        ),
                        (
                            "dep-b".to_string(),
                            WithOriginalConfigurations {
                                value: "b".to_owned(),
                                original_configurations: BTreeSet::from(["cfg(macos)".to_owned()]),
                            },
                        ),
                    ]),
                ),
                (
                    "x86_64-linux".to_owned(),
                    BTreeMap::from([
                        (
                            "dep-a".to_string(),
                            WithOriginalConfigurations {
                                value: "a".to_owned(),
                                original_configurations: BTreeSet::from(["cfg(x86_64)".to_owned()]),
                            },
                        ),
                        (
                            "dep-c".to_string(),
                            WithOriginalConfigurations {
                                value: "c".to_owned(),
                                original_configurations: BTreeSet::from(["cfg(x86_64)".to_owned()]),
                            },
                        ),
                    ]),
                ),
                (
                    "@platforms//os:magic".to_owned(),
                    BTreeMap::from([(
                        "dep-f".to_string(),
                        WithOriginalConfigurations {
                            value: "f".to_owned(),
                            original_configurations: BTreeSet::from([
                                "@platforms//os:magic".to_owned()
                            ]),
                        },
                    )]),
                ),
                (
                    "//another:platform".to_owned(),
                    BTreeMap::from([(
                        "dep-g".to_string(),
                        WithOriginalConfigurations {
                            value: "g".to_owned(),
                            original_configurations: BTreeSet::from([
                                "//another:platform".to_owned()
                            ]),
                        },
                    )]),
                ),
            ]),
            unmapped: BTreeMap::from([(
                "cfg(pdp11)".to_owned(),
                BTreeMap::from([("dep-e".to_string(), "e".to_owned())]),
            )]),
        };

        assert_eq!(select_dict, expected);

        let expected_starlark = indoc! {r#"
            selects.with_unmapped({
                "//another:platform": {
                    "dep-d": "d",
                    "dep-g": "g",  # //another:platform
                },
                "@platforms//os:magic": {
                    "dep-d": "d",
                    "dep-f": "f",  # @platforms//os:magic
                },
                "aarch64-macos": {
                    "dep-a": "a",  # cfg(macos)
                    "dep-b": "b",  # cfg(macos)
                    "dep-d": "d",
                },
                "x86_64-linux": {
                    "dep-a": "a",  # cfg(x86_64)
                    "dep-c": "c",  # cfg(x86_64)
                    "dep-d": "d",
                },
                "x86_64-macos": {
                    "dep-a": "a",  # cfg(macos), cfg(x86_64)
                    "dep-b": "b",  # cfg(macos)
                    "dep-c": "c",  # cfg(x86_64)
                    "dep-d": "d",
                },
                "//conditions:default": {
                    "dep-d": "d",
                },
                selects.NO_MATCHING_PLATFORM_TRIPLES: {
                    "cfg(pdp11)": {
                        "dep-e": "e",
                    },
                },
            })
        "#};

        assert_eq!(
            select_dict.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }
}
