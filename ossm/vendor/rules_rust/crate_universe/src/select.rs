use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Debug;

use serde::{de::DeserializeOwned, Deserialize, Deserializer, Serialize};

/// A wrapper around values where some values may be conditionally included (e.g. only on a certain platform), and others are unconditional.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Select<T>
where
    T: Selectable,
{
    common: T::CommonType,
    selects: BTreeMap<String, T::SelectsType>,
}

pub trait Selectable
where
    Self: SelectableValue,
{
    type ItemType: SelectableValue;
    type CommonType: SelectableValue + Default;
    type SelectsType: SelectableValue;

    fn is_empty(this: &Select<Self>) -> bool;
    fn insert(this: &mut Select<Self>, value: Self::ItemType, configuration: Option<String>);

    fn items(this: &Select<Self>) -> Vec<(Option<String>, Self::ItemType)>;
    fn values(this: &Select<Self>) -> Vec<Self::ItemType>;

    fn merge(lhs: Select<Self>, rhs: Select<Self>) -> Select<Self>;
}

// Replace with `trait_alias` once stabilized.
// https://github.com/rust-lang/rust/issues/41517
pub trait SelectableValue
where
    Self: Debug + Clone + PartialEq + Eq + Serialize + DeserializeOwned,
{
}

impl<T> SelectableValue for T where T: Debug + Clone + PartialEq + Eq + Serialize + DeserializeOwned {}

// Replace with `trait_alias` once stabilized.
// https://github.com/rust-lang/rust/issues/41517
pub trait SelectableOrderedValue
where
    Self: SelectableValue + PartialOrd + Ord,
{
}

impl<T> SelectableOrderedValue for T where T: SelectableValue + PartialOrd + Ord {}

pub(crate) trait SelectableScalar
where
    Self: SelectableValue,
{
}

impl SelectableScalar for String {}
impl SelectableScalar for bool {}
impl SelectableScalar for i64 {}

// General
impl<T> Select<T>
where
    T: Selectable,
{
    pub(crate) fn new() -> Self {
        Self {
            common: T::CommonType::default(),
            selects: BTreeMap::new(),
        }
    }

    pub(crate) fn from_value(value: T::CommonType) -> Self {
        Self {
            common: value,
            selects: BTreeMap::new(),
        }
    }

    /// Whether there zero values in this collection, common or configuration-specific.
    pub fn is_empty(&self) -> bool {
        T::is_empty(self)
    }

    /// A list of the configurations which have some configuration-specific value associated.
    pub fn configurations(&self) -> BTreeSet<String> {
        self.selects.keys().cloned().collect()
    }

    /// All values and their associated configurations, if any.
    pub fn items(&self) -> Vec<(Option<String>, T::ItemType)> {
        T::items(self)
    }

    /// All values, whether common or configured.
    pub fn values(&self) -> Vec<T::ItemType> {
        T::values(self)
    }

    pub(crate) fn insert(&mut self, value: T::ItemType, configuration: Option<String>) {
        T::insert(self, value, configuration);
    }

    pub(crate) fn into_parts(self) -> (T::CommonType, BTreeMap<String, T::SelectsType>) {
        (self.common, self.selects)
    }

    pub(crate) fn merge(lhs: Self, rhs: Self) -> Self {
        T::merge(lhs, rhs)
    }
}

impl<T> Default for Select<T>
where
    T: Selectable,
{
    fn default() -> Self {
        Self::new()
    }
}

impl<'de, T> Deserialize<'de> for Select<T>
where
    T: Selectable,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(Debug, Deserialize)]
        #[serde(untagged)]
        enum Either<T>
        where
            T: Selectable,
        {
            Select {
                common: T::CommonType,
                selects: BTreeMap<String, T::SelectsType>,
            },
            Value(T::CommonType),
        }

        let either = Either::<T>::deserialize(deserializer)?;
        match either {
            Either::Select { common, selects } => Ok(Self { common, selects }),
            Either::Value(common) => Ok(Self {
                common,
                selects: BTreeMap::new(),
            }),
        }
    }
}

// Scalar
impl<T> Selectable for T
where
    T: SelectableScalar,
{
    type ItemType = T;
    type CommonType = Option<Self::ItemType>;
    type SelectsType = Self::ItemType;

    fn is_empty(this: &Select<Self>) -> bool {
        this.common.is_none() && this.selects.is_empty()
    }

    fn items(this: &Select<Self>) -> Vec<(Option<String>, Self::ItemType)> {
        let mut result = Vec::new();
        if let Some(value) = this.common.as_ref() {
            result.push((None, value.clone()));
        }
        result.extend(
            this.selects
                .iter()
                .map(|(configuration, value)| (Some(configuration.clone()), value.clone())),
        );
        result
    }

    fn values(this: &Select<Self>) -> Vec<Self::ItemType> {
        let mut result = Vec::new();
        if let Some(value) = this.common.as_ref() {
            result.push(value.clone());
        }
        result.extend(this.selects.values().cloned());
        result
    }

    fn insert(this: &mut Select<Self>, value: Self::ItemType, configuration: Option<String>) {
        match configuration {
            None => {
                this.selects
                    .retain(|_, existing_value| existing_value != &value);
                this.common = Some(value);
            }
            Some(configuration) => {
                if Some(&value) != this.common.as_ref() {
                    this.selects.insert(configuration, value);
                }
            }
        }
    }

    fn merge(lhs: Select<Self>, rhs: Select<Self>) -> Select<Self> {
        let mut result: Select<Self> = Select::new();

        if let Some(value) = lhs.common {
            result.insert(value, None);
        }
        if let Some(value) = rhs.common {
            result.insert(value, None);
        }

        for (configuration, value) in lhs.selects.into_iter() {
            result.insert(value, Some(configuration));
        }
        for (configuration, value) in rhs.selects.into_iter() {
            result.insert(value, Some(configuration));
        }

        result
    }
}

// Vec<T>
impl<T> Selectable for Vec<T>
where
    T: SelectableValue,
{
    type ItemType = T;
    type CommonType = Vec<T>;
    type SelectsType = Vec<T>;

    fn is_empty(this: &Select<Self>) -> bool {
        this.common.is_empty() && this.selects.is_empty()
    }

    fn items(this: &Select<Self>) -> Vec<(Option<String>, Self::ItemType)> {
        let mut result = Vec::new();
        result.extend(this.common.iter().map(|value| (None, value.clone())));
        result.extend(this.selects.iter().flat_map(|(configuration, values)| {
            values
                .iter()
                .map(|value| (Some(configuration.clone()), value.clone()))
        }));
        result
    }

    fn values(this: &Select<Self>) -> Vec<Self::ItemType> {
        let mut result = Vec::new();
        result.extend(this.common.iter().cloned());
        result.extend(
            this.selects
                .values()
                .flat_map(|values| values.iter().cloned()),
        );
        result
    }

    fn insert(this: &mut Select<Self>, value: Self::ItemType, configuration: Option<String>) {
        match configuration {
            None => this.common.push(value),
            Some(configuration) => this.selects.entry(configuration).or_default().push(value),
        }
    }

    fn merge(lhs: Select<Self>, rhs: Select<Self>) -> Select<Self> {
        let mut result: Select<Self> = Select::new();

        for value in lhs.common.into_iter() {
            result.insert(value, None);
        }
        for value in rhs.common.into_iter() {
            result.insert(value, None);
        }

        for (configuration, values) in lhs.selects.into_iter() {
            for value in values.into_iter() {
                result.insert(value, Some(configuration.clone()));
            }
        }
        for (configuration, values) in rhs.selects.into_iter() {
            for value in values.into_iter() {
                result.insert(value, Some(configuration.clone()));
            }
        }

        result
    }
}

// BTreeSet<T>
impl<T> Selectable for BTreeSet<T>
where
    T: SelectableOrderedValue,
{
    type ItemType = T;
    type CommonType = BTreeSet<T>;
    type SelectsType = BTreeSet<T>;

    fn is_empty(this: &Select<Self>) -> bool {
        this.common.is_empty() && this.selects.is_empty()
    }

    fn items(this: &Select<Self>) -> Vec<(Option<String>, Self::ItemType)> {
        let mut result = Vec::new();
        result.extend(this.common.iter().map(|value| (None, value.clone())));
        result.extend(this.selects.iter().flat_map(|(configuration, values)| {
            values
                .iter()
                .map(|value| (Some(configuration.clone()), value.clone()))
        }));
        result
    }

    fn values(this: &Select<Self>) -> Vec<Self::ItemType> {
        let mut result = Vec::new();
        result.extend(this.common.iter().cloned());
        result.extend(
            this.selects
                .values()
                .flat_map(|values| values.iter().cloned()),
        );
        result
    }

    fn insert(this: &mut Select<Self>, value: Self::ItemType, configuration: Option<String>) {
        match configuration {
            None => {
                this.selects.retain(|_, set| {
                    set.remove(&value);
                    !set.is_empty()
                });
                this.common.insert(value);
            }
            Some(configuration) => {
                if !this.common.contains(&value) {
                    this.selects.entry(configuration).or_default().insert(value);
                }
            }
        }
    }

    fn merge(lhs: Select<Self>, rhs: Select<Self>) -> Select<Self> {
        let mut result: Select<Self> = Select::new();

        for value in lhs.common.into_iter() {
            result.insert(value, None);
        }
        for value in rhs.common.into_iter() {
            result.insert(value, None);
        }

        for (configuration, values) in lhs.selects.into_iter() {
            for value in values {
                result.insert(value, Some(configuration.clone()));
            }
        }
        for (configuration, values) in rhs.selects.into_iter() {
            for value in values {
                result.insert(value, Some(configuration.clone()));
            }
        }

        result
    }
}

impl<T> Select<BTreeSet<T>>
where
    T: SelectableOrderedValue,
{
    pub(crate) fn map<U, F>(self, func: F) -> Select<BTreeSet<U>>
    where
        U: SelectableOrderedValue,
        F: Copy + FnMut(T) -> U,
    {
        Select {
            common: self.common.into_iter().map(func).collect(),
            selects: self
                .selects
                .into_iter()
                .map(|(configuration, values)| {
                    (configuration, values.into_iter().map(func).collect())
                })
                .collect(),
        }
    }
}

// BTreeMap<U, T>
impl<U, T> Selectable for BTreeMap<U, T>
where
    U: SelectableOrderedValue,
    T: SelectableValue,
{
    type ItemType = (U, T);
    type CommonType = BTreeMap<U, T>;
    type SelectsType = BTreeMap<U, T>;

    fn is_empty(this: &Select<Self>) -> bool {
        this.common.is_empty() && this.selects.is_empty()
    }

    fn items(this: &Select<Self>) -> Vec<(Option<String>, Self::ItemType)> {
        let mut result = Vec::new();
        result.extend(
            this.common
                .iter()
                .map(|(key, value)| (None, (key.clone(), value.clone()))),
        );
        result.extend(this.selects.iter().flat_map(|(configuration, values)| {
            values
                .iter()
                .map(|(key, value)| (Some(configuration.clone()), (key.clone(), value.clone())))
        }));
        result
    }

    fn values(this: &Select<Self>) -> Vec<Self::ItemType> {
        let mut result = Vec::new();
        result.extend(
            this.common
                .iter()
                .map(|(key, value)| (key.clone(), value.clone())),
        );
        result.extend(this.selects.values().flat_map(|values| {
            values
                .iter()
                .map(|(key, value)| (key.clone(), value.clone()))
        }));
        result
    }

    fn insert(
        this: &mut Select<Self>,
        (key, value): Self::ItemType,
        configuration: Option<String>,
    ) {
        match configuration {
            None => {
                this.selects.retain(|_, map| {
                    map.remove(&key);
                    !map.is_empty()
                });
                this.common.insert(key, value);
            }
            Some(configuration) => {
                if !this.common.contains_key(&key) {
                    this.selects
                        .entry(configuration)
                        .or_default()
                        .insert(key, value);
                }
            }
        }
    }

    fn merge(lhs: Select<Self>, rhs: Select<Self>) -> Select<Self> {
        let mut result: Select<Self> = Select::new();

        for (key, value) in lhs.common.into_iter() {
            result.insert((key, value), None);
        }
        for (key, value) in rhs.common.into_iter() {
            result.insert((key, value), None);
        }

        for (configuration, entries) in lhs.selects.into_iter() {
            for (key, value) in entries {
                result.insert((key, value), Some(configuration.clone()));
            }
        }
        for (configuration, entries) in rhs.selects.into_iter() {
            for (key, value) in entries {
                result.insert((key, value), Some(configuration.clone()));
            }
        }

        result
    }
}
