use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Debug;

use serde::ser::{SerializeMap, SerializeTupleStruct, Serializer};
use serde::Serialize;
use serde_starlark::{FunctionCall, MULTILINE};

use crate::select::{Select, SelectableOrderedValue};
use crate::utils::starlark::serialize::MultilineArray;
use crate::utils::starlark::{
    looks_like_bazel_configuration_label, NoMatchingPlatformTriples, WithOriginalConfigurations,
};

#[derive(Debug, PartialEq, Eq)]
pub(crate) struct SelectSet<T>
where
    T: SelectableOrderedValue,
{
    // Invariant: any T in `common` is not anywhere in `selects`.
    common: BTreeSet<T>,
    // Invariant: none of the sets are empty.
    selects: BTreeMap<String, BTreeSet<WithOriginalConfigurations<T>>>,
    // Elements from the `Select` whose configuration did not get mapped to any
    // new configuration. They could be ignored, but are preserved here to
    // generate comments that help the user understand what happened.
    unmapped: BTreeMap<String, BTreeSet<T>>,
}

impl<T> SelectSet<T>
where
    T: SelectableOrderedValue,
{
    /// Re-keys the provided Select by the given configuration mapping.
    /// This mapping maps from configurations in the input Select to sets of
    /// configurations in the output SelectSet.
    pub(crate) fn new(
        select: Select<BTreeSet<T>>,
        platforms: &BTreeMap<String, BTreeSet<String>>,
    ) -> Self {
        let (common, selects) = select.into_parts();

        // Map new configuration -> value -> old configurations.
        let mut remapped: BTreeMap<String, BTreeMap<T, BTreeSet<String>>> = BTreeMap::new();
        // Map unknown configuration -> value.
        let mut unmapped: BTreeMap<String, BTreeSet<T>> = BTreeMap::new();

        for (original_configuration, values) in selects {
            match platforms.get(&original_configuration) {
                Some(configurations) => {
                    for configuration in configurations {
                        for value in &values {
                            remapped
                                .entry(configuration.clone())
                                .or_default()
                                .entry(value.clone())
                                .or_default()
                                .insert(original_configuration.clone());
                        }
                    }
                }
                None => {
                    if looks_like_bazel_configuration_label(&original_configuration) {
                        let destination =
                            remapped.entry(original_configuration.clone()).or_default();
                        for value in values {
                            destination
                                .entry(value)
                                .or_default()
                                .insert(original_configuration.clone());
                        }
                    } else {
                        unmapped
                            .entry(original_configuration.clone())
                            .or_default()
                            .extend(values.into_iter());
                    };
                }
            }
        }

        Self {
            common,
            selects: remapped
                .into_iter()
                .map(|(new_configuration, value_to_original_configuration)| {
                    (
                        new_configuration,
                        value_to_original_configuration
                            .into_iter()
                            .map(
                                |(value, original_configurations)| WithOriginalConfigurations {
                                    value,
                                    original_configurations,
                                },
                            )
                            .collect(),
                    )
                })
                .collect(),
            unmapped,
        }
    }

    /// Determine whether or not the select should be serialized
    pub(crate) fn is_empty(&self) -> bool {
        self.common.is_empty() && self.selects.is_empty() && self.unmapped.is_empty()
    }
}

impl<T> Serialize for SelectSet<T>
where
    T: SelectableOrderedValue,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // Output looks like:
        //
        //     [
        //         "common...",
        //     ] + select({
        //         "configuration": [
        //             "value...",  # cfg(whatever)
        //         ],
        //         "//conditions:default": [],
        //     })
        //
        // The common part and select are each omitted if they are empty (except
        // if the entire thing is empty, in which case we serialize the common
        // part to get an empty array).
        //
        // If there are unmapped entries, we include them like this:
        //
        //     [
        //         "common...",
        //     ] + selects.with_unmapped({
        //         "configuration": [
        //             "value...",  # cfg(whatever)
        //         ],
        //         "//conditions:default": [],
        //         selects.NO_MATCHING_PLATFORM_TRIPLES: {
        //             "cfg(obscure)": [
        //                 "value...",
        //             ],
        //         },
        //     })

        let mut plus = serializer.serialize_tuple_struct("+", MULTILINE)?;

        if !self.common.is_empty() || self.selects.is_empty() && self.unmapped.is_empty() {
            plus.serialize_field(&MultilineArray(&self.common))?;
        }

        if !self.selects.is_empty() || !self.unmapped.is_empty() {
            struct SelectInner<'a, T: Ord>(&'a SelectSet<T>)
            where
                T: SelectableOrderedValue;

            impl<T> Serialize for SelectInner<'_, T>
            where
                T: SelectableOrderedValue,
            {
                fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
                where
                    S: Serializer,
                {
                    let mut map = serializer.serialize_map(Some(MULTILINE))?;
                    for (cfg, value) in &self.0.selects {
                        map.serialize_entry(cfg, &MultilineArray(value))?;
                    }
                    map.serialize_entry("//conditions:default", &[] as &[T])?;
                    if !self.0.unmapped.is_empty() {
                        struct SelectUnmapped<'a, T>(&'a BTreeMap<String, BTreeSet<T>>)
                        where
                            T: SelectableOrderedValue;

                        impl<T> Serialize for SelectUnmapped<'_, T>
                        where
                            T: SelectableOrderedValue,
                        {
                            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
                            where
                                S: Serializer,
                            {
                                let mut map = serializer.serialize_map(Some(MULTILINE))?;
                                for (cfg, values) in self.0.iter() {
                                    map.serialize_entry(cfg, &MultilineArray(values))?;
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

            plus.serialize_field(&FunctionCall::new(function, [SelectInner(self)]))?;
        }

        plus.end()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use indoc::indoc;

    #[test]
    fn empty_select_set() {
        let select_set: SelectSet<String> = SelectSet::new(Default::default(), &Default::default());

        let expected_starlark = indoc! {r#"
            []
        "#};

        assert_eq!(
            select_set.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn no_platform_specific_select_set() {
        let mut select: Select<BTreeSet<String>> = Select::default();
        select.insert("Hello".to_owned(), None);

        let select_set = SelectSet::new(select, &Default::default());

        let expected_starlark = indoc! {r#"
            [
                "Hello",
            ]
        "#};

        assert_eq!(
            select_set.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn only_platform_specific_select_set() {
        let mut select: Select<BTreeSet<String>> = Select::default();
        select.insert("Hello".to_owned(), Some("platform".to_owned()));

        let platforms = BTreeMap::from([(
            "platform".to_owned(),
            BTreeSet::from(["platform".to_owned()]),
        )]);

        let select_set = SelectSet::new(select, &platforms);

        let expected_starlark = indoc! {r#"
            select({
                "platform": [
                    "Hello",  # platform
                ],
                "//conditions:default": [],
            })
        "#};

        assert_eq!(
            select_set.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn mixed_select_set() {
        let mut select: Select<BTreeSet<String>> = Select::default();
        select.insert("Hello".to_owned(), Some("platform".to_owned()));
        select.insert("Goodbye".to_owned(), None);

        let platforms = BTreeMap::from([(
            "platform".to_owned(),
            BTreeSet::from(["platform".to_owned()]),
        )]);

        let select_set = SelectSet::new(select, &platforms);

        let expected_starlark = indoc! {r#"
            [
                "Goodbye",
            ] + select({
                "platform": [
                    "Hello",  # platform
                ],
                "//conditions:default": [],
            })
        "#};

        assert_eq!(
            select_set.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }

    #[test]
    fn remap_select_set_configurations() {
        let mut select: Select<BTreeSet<String>> = Select::default();
        select.insert("dep-a".to_owned(), Some("cfg(macos)".to_owned()));
        select.insert("dep-b".to_owned(), Some("cfg(macos)".to_owned()));
        select.insert("dep-d".to_owned(), Some("cfg(macos)".to_owned()));
        select.insert("dep-a".to_owned(), Some("cfg(x86_64)".to_owned()));
        select.insert("dep-c".to_owned(), Some("cfg(x86_64)".to_owned()));
        select.insert("dep-e".to_owned(), Some("cfg(pdp11)".to_owned()));
        select.insert("dep-d".to_owned(), None);
        select.insert("dep-f".to_owned(), Some("@platforms//os:magic".to_owned()));
        select.insert("dep-g".to_owned(), Some("//another:platform".to_owned()));

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

        let select_set = SelectSet::new(select, &platforms);

        let expected = SelectSet {
            common: BTreeSet::from(["dep-d".to_owned()]),
            selects: BTreeMap::from([
                (
                    "x86_64-macos".to_owned(),
                    BTreeSet::from([
                        WithOriginalConfigurations {
                            value: "dep-a".to_owned(),
                            original_configurations: BTreeSet::from([
                                "cfg(macos)".to_owned(),
                                "cfg(x86_64)".to_owned(),
                            ]),
                        },
                        WithOriginalConfigurations {
                            value: "dep-b".to_owned(),
                            original_configurations: BTreeSet::from(["cfg(macos)".to_owned()]),
                        },
                        WithOriginalConfigurations {
                            value: "dep-c".to_owned(),
                            original_configurations: BTreeSet::from(["cfg(x86_64)".to_owned()]),
                        },
                    ]),
                ),
                (
                    "aarch64-macos".to_owned(),
                    BTreeSet::from([
                        WithOriginalConfigurations {
                            value: "dep-a".to_owned(),
                            original_configurations: BTreeSet::from(["cfg(macos)".to_owned()]),
                        },
                        WithOriginalConfigurations {
                            value: "dep-b".to_owned(),
                            original_configurations: BTreeSet::from(["cfg(macos)".to_owned()]),
                        },
                    ]),
                ),
                (
                    "x86_64-linux".to_owned(),
                    BTreeSet::from([
                        WithOriginalConfigurations {
                            value: "dep-a".to_owned(),
                            original_configurations: BTreeSet::from(["cfg(x86_64)".to_owned()]),
                        },
                        WithOriginalConfigurations {
                            value: "dep-c".to_owned(),
                            original_configurations: BTreeSet::from(["cfg(x86_64)".to_owned()]),
                        },
                    ]),
                ),
                (
                    "@platforms//os:magic".to_owned(),
                    BTreeSet::from([WithOriginalConfigurations {
                        value: "dep-f".to_owned(),
                        original_configurations: BTreeSet::from(
                            ["@platforms//os:magic".to_owned()],
                        ),
                    }]),
                ),
                (
                    "//another:platform".to_owned(),
                    BTreeSet::from([WithOriginalConfigurations {
                        value: "dep-g".to_owned(),
                        original_configurations: BTreeSet::from(["//another:platform".to_owned()]),
                    }]),
                ),
            ]),
            unmapped: BTreeMap::from([(
                "cfg(pdp11)".to_owned(),
                BTreeSet::from(["dep-e".to_owned()]),
            )]),
        };

        assert_eq!(select_set, expected);

        let expected_starlark = indoc! {r#"
            [
                "dep-d",
            ] + selects.with_unmapped({
                "//another:platform": [
                    "dep-g",  # //another:platform
                ],
                "@platforms//os:magic": [
                    "dep-f",  # @platforms//os:magic
                ],
                "aarch64-macos": [
                    "dep-a",  # cfg(macos)
                    "dep-b",  # cfg(macos)
                ],
                "x86_64-linux": [
                    "dep-a",  # cfg(x86_64)
                    "dep-c",  # cfg(x86_64)
                ],
                "x86_64-macos": [
                    "dep-a",  # cfg(macos), cfg(x86_64)
                    "dep-b",  # cfg(macos)
                    "dep-c",  # cfg(x86_64)
                ],
                "//conditions:default": [],
                selects.NO_MATCHING_PLATFORM_TRIPLES: {
                    "cfg(pdp11)": [
                        "dep-e",
                    ],
                },
            })
        "#};

        assert_eq!(
            select_set.serialize(serde_starlark::Serializer).unwrap(),
            expected_starlark,
        );
    }
}
