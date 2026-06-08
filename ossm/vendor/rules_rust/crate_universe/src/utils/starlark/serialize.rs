use serde::ser::{SerializeSeq, SerializeStruct, SerializeTupleStruct, Serializer};
use serde::Serialize;
use serde_starlark::{FunctionCall, MULTILINE, ONELINE};

use super::{
    Data, ExportsFiles, License, Load, Package, PackageInfo, RustBinary, RustLibrary, RustProcMacro,
};

// For structs that contain #[serde(flatten)], a quirk of how Serde processes
// that attribute is that they get serialized as a map, not struct. In Starlark
// unlike in JSON, maps and structs are differently serialized, so we need to
// help fill in the function name or else we'd get a Starlark map instead.
pub(crate) fn rust_proc_macro<S>(rule: &RustProcMacro, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    FunctionCall::new("rust_proc_macro", rule).serialize(serializer)
}

pub(crate) fn rust_library<S>(rule: &RustLibrary, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    FunctionCall::new("rust_library", rule).serialize(serializer)
}

pub(crate) fn rust_binary<S>(rule: &RustBinary, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    FunctionCall::new("rust_binary", rule).serialize(serializer)
}

// Serialize an array with each element on its own line, even if there is just a
// single element which serde_starlark would ordinarily place on the same line
// as the array brackets.
pub(crate) struct MultilineArray<'a, A>(pub(crate) &'a A);

impl<'a, A, T> Serialize for MultilineArray<'a, A>
where
    &'a A: IntoIterator<Item = &'a T>,
    T: Serialize + 'a,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut array = serializer.serialize_seq(Some(serde_starlark::MULTILINE))?;
        for element in self.0 {
            array.serialize_element(element)?;
        }
        array.end()
    }
}

impl Serialize for Load {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let line = if self.items.len() > 1 {
            MULTILINE
        } else {
            ONELINE
        };
        let mut call = serializer.serialize_tuple_struct("load", line)?;
        call.serialize_field(&self.bzl)?;
        for item in &self.items {
            call.serialize_field(item)?;
        }
        call.end()
    }
}

impl Serialize for Package {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let has_metadata = !self.default_package_metadata.is_empty();
        let mut call = serializer
            .serialize_struct("package", if has_metadata { MULTILINE } else { ONELINE })?;
        if has_metadata {
            call.serialize_field("default_package_metadata", &self.default_package_metadata)?;
        }
        call.serialize_field("default_visibility", &self.default_visibility)?;
        call.end()
    }
}

impl Serialize for PackageInfo {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut call = serializer.serialize_struct("package_info", MULTILINE)?;
        call.serialize_field("name", &self.name)?;
        call.serialize_field("package_name", &self.package_name)?;
        call.serialize_field("package_version", &self.package_version)?;
        call.serialize_field("package_url", &self.package_url)?;
        call.end()
    }
}

impl Serialize for License {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut call = serializer.serialize_struct("license", MULTILINE)?;
        call.serialize_field("name", &self.name)?;
        call.serialize_field("license_kinds", &self.license_kinds)?;
        if !self.license_text.is_empty() {
            call.serialize_field("license_text", &self.license_text)?;
        }
        call.end()
    }
}

impl Serialize for ExportsFiles {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut call = serializer.serialize_tuple_struct("exports_files", MULTILINE)?;
        call.serialize_field(&FunctionCall::new("+", (&self.paths, &self.globs)))?;
        call.end()
    }
}

impl Data {
    pub(crate) fn is_empty(&self) -> bool {
        self.glob.has_any_include() && self.select.is_empty()
    }
}

impl Serialize for Data {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut plus = serializer.serialize_tuple_struct("+", MULTILINE)?;
        if !self.glob.has_any_include() {
            plus.serialize_field(&self.glob)?;
        }
        if !self.select.is_empty() || self.glob.has_any_include() {
            plus.serialize_field(&self.select)?;
        }
        plus.end()
    }
}
