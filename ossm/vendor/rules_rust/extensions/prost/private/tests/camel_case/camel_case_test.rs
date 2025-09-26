//! Tests that strange casings of message names are handled correctly.

use another_proto::another::Another;

#[test]
fn test_nested_messages() {
    let _a = Another::default();
}
