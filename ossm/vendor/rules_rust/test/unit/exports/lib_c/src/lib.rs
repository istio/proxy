// This symbol is an export that `lib_b` exports are re-exports from `lib_a`.
use lib_b::greeting_from;

pub fn greeting_c() -> String {
    greeting_from("lib_c")
}

#[cfg(test)]
mod test {
    use super::*;

    use lib_b::{greeting_a, greeting_b};

    #[test]
    fn test_all_greetings() {
        assert_eq!(greeting_a(), "Hello from lib_a!".to_owned());
        assert_eq!(greeting_b(), "Hello from lib_b!".to_owned());
        assert_eq!(greeting_c(), "Hello from lib_c!".to_owned());
    }
}
