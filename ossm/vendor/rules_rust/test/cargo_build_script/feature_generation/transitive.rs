//! A consumer of the `lib` crate which relies on a `build.rs` produced feature.

pub fn data() -> String {
    String::from(lib::DATA)
}
