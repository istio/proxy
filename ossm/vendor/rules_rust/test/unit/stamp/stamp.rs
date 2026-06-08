//! A small test library for ensuring `--stamp` data is correctly set at compile time.

pub fn build_timestamp() -> &'static str {
    env!("BUILD_TIMESTAMP")
}

pub fn build_user() -> &'static str {
    env!("BUILD_USER")
}

#[cfg(test)]
mod test {
    use super::*;

    #[cfg(any(
        feature = "always_stamp",
        feature = "consult_cmdline_value_is_true",
        feature = "always_stamp_build_flag_true",
        feature = "always_stamp_build_flag_false"
    ))]
    #[test]
    fn stamp_resolved_for_library() {
        assert!(!build_timestamp().contains("BUILD_TIMESTAMP"));
        assert!(build_timestamp().chars().all(char::is_numeric));

        assert!(!build_user().contains("BUILD_USER"));
    }

    #[cfg(any(
        feature = "always_stamp",
        feature = "consult_cmdline_value_is_true",
        feature = "always_stamp_build_flag_true",
        feature = "always_stamp_build_flag_false"
    ))]
    #[test]
    fn stamp_resolved_for_test() {
        assert!(!env!("BUILD_TIMESTAMP").contains("BUILD_TIMESTAMP"));
        assert!(env!("BUILD_TIMESTAMP").chars().all(char::is_numeric));

        assert!(!env!("BUILD_USER").contains("BUILD_USER"));
    }

    #[cfg(any(
        feature = "never_stamp",
        feature = "consult_cmdline_value_is_false",
        feature = "never_stamp_build_flag_true",
        feature = "never_stamp_build_flag_false"
    ))]
    #[test]
    fn stamp_not_resolved_for_library() {
        assert!(build_timestamp().contains("BUILD_TIMESTAMP"));
        assert!(build_user().contains("BUILD_USER"));
    }

    #[cfg(any(
        feature = "never_stamp",
        feature = "consult_cmdline_value_is_false",
        feature = "never_stamp_build_flag_true",
        feature = "never_stamp_build_flag_false"
    ))]
    #[test]
    fn stamp_not_resolved_for_test() {
        assert!(env!("BUILD_TIMESTAMP").contains("BUILD_TIMESTAMP"));
        assert!(env!("BUILD_USER").contains("BUILD_USER"));
    }
}
