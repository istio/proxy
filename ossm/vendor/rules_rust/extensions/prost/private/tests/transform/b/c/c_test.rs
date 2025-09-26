//! Tests transitive dependencies.

use std::collections::HashSet;

use c_proto::a::b::c::C;

#[test]
fn test_c() {
    let c = C {
        name: "c".to_string(),
    };

    // This shows that Hash and Eq are implemented for C
    let set = HashSet::from([c]);
    assert_eq!(set.len(), 1, "{:#?}", set);
}
