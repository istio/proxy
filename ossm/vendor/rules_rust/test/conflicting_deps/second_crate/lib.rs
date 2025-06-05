// This crate's name conflicts with its dependent but this should work OK.

pub fn example_conflicting_symbol() -> String {
    "[from second_crate]".to_owned()
}
