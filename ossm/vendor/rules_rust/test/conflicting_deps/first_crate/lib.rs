// This crate's name conflicts with its dependency but this should work OK.

extern crate example_name_conflict;

pub fn example_conflicting_symbol() -> String {
    format!(
        "[from first_crate] -> {}",
        example_name_conflict::example_conflicting_symbol()
    )
}
