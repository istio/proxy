//! Consumer of build.rs generated feature.

#[cfg(build_rs_generated_feature)]
pub const DATA: &str = "La-Li-Lu-Le-Lo";

#[cfg(not(build_rs_generated_feature))]
pub const DATA: &str = "hello-world";
