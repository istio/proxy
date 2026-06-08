use types_proto::Enum;
use types_proto::{types, Types};

#[test]
fn test_types() {
    Types {
        a_enum: Enum::C.into(),
        a_double: 2.0,
        a_float: 3.0,
        a_int32: 4,
        a_int64: 5,
        a_uint32: 6,
        a_uint64: 7,
        a_sint32: 8,
        a_sint64: 9,
        a_fixed32: 10,
        a_fixed64: 11,
        a_sfixed32: 12,
        a_sfixed64: 13,
        a_bool: true,
        a_string: "a".to_string(),
        a_bytes: vec![1, 2, 3],
        repeated_enum: vec![Enum::A.into(), Enum::B.into()],
        repeated_double: vec![2.0, 3.0],
        repeated_float: vec![3.0, 4.0],
        repeated_int32: vec![4, 5],
        repeated_int64: vec![5, 6],
        repeated_uint32: vec![6, 7],
        repeated_uint64: vec![7, 8],
        repeated_sint32: vec![8, 9],
        repeated_sint64: vec![9, 10],
        repeated_fixed32: vec![10, 11],
        repeated_fixed64: vec![11, 12],
        repeated_sfixed32: vec![12, 13],
        repeated_sfixed64: vec![13, 14],
        repeated_bool: vec![true, false],
        repeated_string: vec!["a".to_string(), "b".to_string()],
        repeated_bytes: vec![vec![1, 2, 3], vec![4, 5, 6]],
        map_string_enum: vec![
            ("a".to_string(), Enum::A.into()),
            ("b".to_string(), Enum::B.into()),
        ]
        .into_iter()
        .collect(),
        map_string_double: vec![("a".to_string(), 2.0), ("b".to_string(), 3.0)]
            .into_iter()
            .collect(),
        map_string_float: vec![("a".to_string(), 3.0), ("b".to_string(), 4.0)]
            .into_iter()
            .collect(),
        map_string_int32: vec![("a".to_string(), 4), ("b".to_string(), 5)]
            .into_iter()
            .collect(),
        map_string_int64: vec![("a".to_string(), 5), ("b".to_string(), 6)]
            .into_iter()
            .collect(),
        map_string_uint32: vec![("a".to_string(), 6), ("b".to_string(), 7)]
            .into_iter()
            .collect(),
        map_string_uint64: vec![("a".to_string(), 7), ("b".to_string(), 8)]
            .into_iter()
            .collect(),
        map_string_sint32: vec![("a".to_string(), 8), ("b".to_string(), 9)]
            .into_iter()
            .collect(),
        map_string_sint64: vec![("a".to_string(), 9), ("b".to_string(), 10)]
            .into_iter()
            .collect(),
        map_string_fixed32: vec![("a".to_string(), 10), ("b".to_string(), 11)]
            .into_iter()
            .collect(),
        map_string_fixed64: vec![("a".to_string(), 11), ("b".to_string(), 12)]
            .into_iter()
            .collect(),
        map_string_sfixed32: vec![("a".to_string(), 12), ("b".to_string(), 13)]
            .into_iter()
            .collect(),
        map_string_sfixed64: vec![("a".to_string(), 13), ("b".to_string(), 14)]
            .into_iter()
            .collect(),
        map_string_bool: vec![("a".to_string(), true), ("b".to_string(), false)]
            .into_iter()
            .collect(),
        map_string_string: vec![
            ("a".to_string(), "a".to_string()),
            ("b".to_string(), "b".to_string()),
        ]
        .into_iter()
        .collect(),
        map_string_bytes: vec![
            ("a".to_string(), vec![1, 2, 3]),
            ("b".to_string(), vec![4, 5, 6]),
        ]
        .into_iter()
        .collect(),
        one_of: Some(types::OneOf::OneofFloat(1.0)),
    };
}
