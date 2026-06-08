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
                duration: Some(duration),
                ..Default::default()
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

    let b_ref = a.b.as_ref().unwrap();
    let c_ref = b_ref.c.as_ref().unwrap();

    assert_eq!(a.name, "a");
    assert_eq!(b_ref.name, "b");
    assert_eq!(c_ref.name, "c");
    assert_eq!(a.duration, c_ref.duration);
}
