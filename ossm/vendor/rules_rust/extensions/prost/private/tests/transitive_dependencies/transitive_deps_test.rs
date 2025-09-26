//! Tests transitive dependencies reexport compatibility
//!
//! a_proto reexports b_proto, which reexports c_proto.
//! a_proto::b_proto's B should be the exact same as b_proto's B.

#[test]
fn test_reexports() {
    assert_eq!(
        a_proto::b_proto::a::b::B::default(),
        b_proto::a::b::B::default()
    );

    let c_from_reexport = a_proto::b_proto::c_proto::a::b::c::C {
        name: "c".to_string(),
        any: Default::default(),
        duration: Some(duration_proto::google::protobuf::Duration {
            seconds: 1,
            nanos: 0,
        }),
    };

    // Ensure they're compatible. This would fail to compile if `C` was not using the same transitive dependency.
    let b = b_proto::a::b::B {
        name: "b".to_string(),
        empty: Default::default(),
        c: Some(c_from_reexport),
    };
    assert_eq!(b.name, "b");
}
