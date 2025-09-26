//! Tests transitive dependencies.

use c_proto::a::b::c::C;
use c_proto::any_proto::google::protobuf::Any;
use c_proto::duration_proto::google::protobuf::Duration;

#[test]
fn test_c() {
    let c = C {
        name: "c".to_string(),
        any: Some(Any::default()),
        duration: Some(Duration {
            seconds: 1,
            nanos: 0,
        }),
    };

    assert_eq!(c.name, "c");
    assert_eq!(c.duration.unwrap().seconds, 1);
}
