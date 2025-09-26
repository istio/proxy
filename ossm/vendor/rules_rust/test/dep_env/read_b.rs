use std::env::var;

fn main() {
    assert_eq!(var("DEP_Y_B").unwrap().trim(), "b_value");
}
