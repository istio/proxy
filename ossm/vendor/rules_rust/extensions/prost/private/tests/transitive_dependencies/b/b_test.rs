//! Tests transitive dependencies.

use b_proto::a::b::B;
use b_proto::c_proto::a::b::c::C;

#[test]
fn test_b() {
    let b = B {
        name: "b".to_string(),
        c: Some(C {
            name: "c".to_string(),
            ..Default::default()
        }),
        ..Default::default()
    };

    assert_eq!(b.name, "b");
    assert_eq!(b.c.unwrap().name, "c");
}
