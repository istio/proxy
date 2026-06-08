//! Tests transitive dependencies.

use b_proto::a::b::B;
use b_proto::c_proto::a::b::c::C;

use greeting::Greeting;

#[test]
fn test_b() {
    let b = B {
        name: "b".to_string(),
        c: Some(C {
            name: "c".to_string(),
        }),
        ..Default::default()
    };

    assert_eq!("Hallo, Bazel, my name is B!", b.greet("Bazel"));
}
