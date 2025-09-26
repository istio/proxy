// The functionality from `lib_a` should be usable within this crate
pub use lib_a::{greeting_a, greeting_from};

pub fn greeting_b() -> String {
    greeting_from("lib_b")
}
