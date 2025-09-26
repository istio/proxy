//! Utilities for parsing [workspace status stamps](https://bazel.build/docs/user-manual#workspace-status).

/// The error type of workspace status parsing.
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum WorkspaceStatusError {
    /// The workspace status data is malformed and cannot be parsed.
    InvalidFormat(String),
}

/// Returns an iterator of workspace status stamp values parsed from the given text.
pub fn parse_workspace_status_stamps(
    text: &'_ str,
) -> impl Iterator<Item = Result<(&'_ str, &'_ str), WorkspaceStatusError>> {
    text.lines().map(|l| {
        let pair = l.split_once(' ');
        pair.ok_or_else(|| {
            WorkspaceStatusError::InvalidFormat(format!(
                "Invalid workspace status stamp value:\n{}",
                l
            ))
        })
    })
}

#[cfg(test)]
mod test {
    use super::*;

    use std::collections::{BTreeMap, HashMap};

    #[test]
    fn test_parse_into_btree_map() {
        let status = [
            "BUILD_TIMESTAMP 1730574875",
            "BUILD_USER user name",
            "STABLE_STAMP_VALUE stable",
        ]
        .join("\n");

        let stamps = parse_workspace_status_stamps(&status)
            .flatten()
            .collect::<BTreeMap<_, _>>();
        assert_eq!(
            BTreeMap::from([
                ("BUILD_TIMESTAMP", "1730574875"),
                ("BUILD_USER", "user name"),
                ("STABLE_STAMP_VALUE", "stable"),
            ]),
            stamps
        );
    }

    #[test]
    fn test_parse_into_hash_map() {
        let status = [
            "BUILD_TIMESTAMP 1730574875",
            "BUILD_USER user name",
            "STABLE_STAMP_VALUE stable",
        ]
        .join("\n");

        let stamps = parse_workspace_status_stamps(&status)
            .flatten()
            .collect::<HashMap<_, _>>();
        assert_eq!(
            HashMap::from([
                ("BUILD_TIMESTAMP", "1730574875"),
                ("BUILD_USER", "user name"),
                ("STABLE_STAMP_VALUE", "stable"),
            ]),
            stamps
        );
    }

    #[test]
    fn test_chain() {
        let stable_status =
            ["STABLE_STAMP_VALUE1 stable1", "STABLE_STAMP_VALUE2 stable2"].join("\n");

        let volatile_status = [
            "VOLATILE_STAMP_VALUE1 volatile1",
            "VOLATILE_STAMP_VALUE2 volatile2",
        ]
        .join("\n");

        let stamps = parse_workspace_status_stamps(&stable_status)
            .chain(parse_workspace_status_stamps(&volatile_status))
            .flatten()
            .collect::<BTreeMap<_, _>>();

        assert_eq!(
            BTreeMap::from([
                ("STABLE_STAMP_VALUE1", "stable1"),
                ("STABLE_STAMP_VALUE2", "stable2"),
                ("VOLATILE_STAMP_VALUE1", "volatile1"),
                ("VOLATILE_STAMP_VALUE2", "volatile2"),
            ]),
            stamps
        );
    }

    #[test]
    fn test_parse_invalid_stamps() {
        let status = [
            "BUILD_TIMESTAMP 1730574875",
            "BUILD_USERusername",
            "STABLE_STAMP_VALUE stable",
        ]
        .join("\n");

        let error = parse_workspace_status_stamps(&status)
            .find_map(|result| result.err())
            .expect("No error found when one was expected");
        assert_eq!(
            WorkspaceStatusError::InvalidFormat(
                "Invalid workspace status stamp value:\nBUILD_USERusername".to_owned()
            ),
            error
        );
    }
}
