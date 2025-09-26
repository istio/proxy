//! Tests transitive dependencies.

use a_proto::a::A;
use a_proto::b_proto::c_proto::a::b::c::C;
use a_proto::duration_proto::google::protobuf::Duration;
use a_proto::timestamp_proto::google::protobuf::Timestamp;
use a_proto::types_proto::Types;

#[test]
fn test_a() {
    let duration = Duration {
        seconds: 1,
        nanos: 2,
    };

    let a = A {
        name: "a".to_string(),
        // Ensure the external `b_proto` dependency is compatible with `a_proto`'s `B`.
        b: Some(b_proto::a::b::B {
            name: "b".to_string(),
            c: Some(C {
                name: "c".to_string(),
            }),
            ..Default::default()
        }),
        timestamp: Some(Timestamp {
            seconds: 1,
            nanos: 2,
        }),
        duration: Some(duration),
        types: Some(Types::default()),
    };

    assert_eq!(
        "Display of: A",
        format!("{}", a),
        "Unexpected `Display` implementation for {:#?}",
        a
    );
}

#[test]
fn test_b() {
    use b_proto::Greeting;

    let b = b_proto::a::b::B {
        name: "b".to_string(),
        c: Some(C {
            name: "c".to_string(),
        }),
        ..Default::default()
    };

    assert_eq!("Hallo, Bazel, my name is B!", b.greet("Bazel"));
}
