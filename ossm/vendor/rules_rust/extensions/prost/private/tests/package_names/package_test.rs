//! Tests variations of the package name, including empty package names.

use pkg_a_b_proto::pkg::a::b::Message as PkgABMessage;
use pkg_a_proto::pkg::a::Message as PkgAMessage;
use pkg_empty_proto::Message as PkgEmptyMessage;
use pkg_proto::pkg::Message as PkgMessage;

#[test]
fn test_packages() {
    let pkg = PkgMessage {
        name: "pkg".to_string(),
    };
    let pkg_a = PkgAMessage {
        name: "pkg_a".to_string(),
    };
    let pkg_a_b = PkgABMessage {
        name: "pkg_a_b".to_string(),
    };
    let pkg_empty = PkgEmptyMessage {
        name: "pkg_empty".to_string(),
    };

    assert_eq!(pkg.name, "pkg");
    assert_eq!(pkg_a.name, "pkg_a");
    assert_eq!(pkg_a_b.name, "pkg_a_b");
    assert_eq!(pkg_empty.name, "pkg_empty");
}
