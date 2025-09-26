//! Tests the usage of nested messages.

use nested_messages_proto::nested_messages::a::b::{C, D};
use nested_messages_proto::nested_messages::a::B;
use nested_messages_proto::nested_messages::A;

#[test]
fn test_nested_messages() {
    let a = A {
        name: "a".to_string(),
        b: Some(B {
            name: "b".to_string(),
            c: Some(C {
                name: "c".to_string(),
            }),
            d: D::E.into(),
        }),
    };

    assert_eq!(a.name, "a");
}
