use std::env::var;

fn main() {
    assert!(var("DEP_X_A").is_err());
    assert_eq!(var("DEP_Y_C").unwrap(), "c_value");
}
