extern crate example_name_conflict;

// This crate depends on a pair of dependencies (transitively) that have the same name. This should
// work OK.

pub fn example_conflicting_symbol() -> String {
    format!(
        "[from main lib] -> {}",
        example_name_conflict::example_conflicting_symbol()
    )
}

#[cfg(test)]
mod tests {
    #[test]
    fn symbols_all_resolve_correctly() {
        assert_eq!(
            ::example_conflicting_symbol(),
            "[from main lib] -> [from first_crate] -> [from second_crate]".to_owned()
        );
    }
}
