use std::env::var;

fn main() {
    assert_eq!(var("DEP_X_A").unwrap(), "a_value");
}
