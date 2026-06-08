//! Tests the Google well-known types.

use nocompile_well_known_types_proto::wkt::WellKnownTypes;
use prost_types::{
    compiler::Version,
    field::{Cardinality, Kind},
    Any, Api, DescriptorProto, Duration, Field, FieldMask, Method, Mixin, Option, SourceContext,
    Struct, Syntax, Timestamp, Type, Value,
};

#[test]
fn test_well_known_types() {
    let wkt = WellKnownTypes {
        any: Some(Any {
            type_url: "type.googleapis.com/google.protobuf.Any".to_string(),
            value: vec![],
        }),
        api: Some(Api {
            name: "api".to_string(),
            methods: vec![Method {
                name: "method".to_string(),
                ..Method::default()
            }],
            options: vec![Option {
                name: "option".to_string(),
                ..Option::default()
            }],
            version: "1.2.3".to_string(),
            source_context: Some(SourceContext {
                file_name: "file_name".to_string(),
            }),
            mixins: vec![Mixin {
                name: "mixin".to_string(),
                ..Mixin::default()
            }],
            syntax: Syntax::Proto3.into(),
        }),
        method: Some(Method {
            name: "method".to_string(),
            ..Method::default()
        }),
        mixin: Some(Mixin {
            name: "mixin".to_string(),
            ..Mixin::default()
        }),
        compiler_version: Some(Version {
            major: Some(1),
            minor: Some(2),
            patch: Some(3),
            suffix: Some("suffix".to_string()),
        }),
        descriptor_proto: Some(DescriptorProto::default()),
        empty: Some(()),
        duration: Some(Duration {
            seconds: 1,
            nanos: 2,
        }),
        field_mask: Some(FieldMask {
            paths: vec!["path".to_string()],
        }),
        source_context: Some(SourceContext {
            file_name: "file_name".to_string(),
        }),
        r#struct: Some(Struct {
            fields: vec![("field".to_string(), Value::default())]
                .into_iter()
                .collect(),
        }),
        timestamp: Some(Timestamp {
            seconds: 1,
            nanos: 2,
        }),
        r#type: Some(Type {
            name: "type".to_string(),
            fields: vec![Field {
                kind: Kind::TypeDouble.into(),
                cardinality: Cardinality::Required.into(),
                ..Field::default()
            }],
            ..Type::default()
        }),
        bool_value: Some(true),
        bytes_value: Some(vec![]),
        double_value: Some(1.0),
        float_value: Some(1.1),
        int32_value: Some(2),
        int64_value: Some(3),
        string_value: Some("value".to_string()),
        uint32_value: Some(4),
        uint64_value: Some(5),
    };

    assert!(wkt.any.is_some());
}
