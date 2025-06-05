//! Tests protos with various capitalizations in their package names are
//! consumable in an expected way.

use bar_proto::b_ar::b_az::qaz::qu_x::bar::Baz as BazMessage;
use bar_proto::b_ar::b_az::qaz::qu_x::Bar as BarMessage;
use bar_proto::foo_proto::foo::quu_x::co_rg_e::grault::ga_rply::foo::NestedFoo as NestedFooMessage;
use bar_proto::foo_proto::foo::quu_x::co_rg_e::grault::ga_rply::Foo as FooMessage;

#[test]
fn test_packages() {
    let bar_message = BarMessage {
        name: "bar".to_string(),
        foo: Some(FooMessage {
            name: "foo".to_string(),
        }),
        nested_foo: Some(NestedFooMessage {
            name: "nested_foo".to_string(),
        }),
    };
    let baz_message = BazMessage {
        name: "baz".to_string(),
    };

    assert_eq!(bar_message.name, "bar");
    assert_eq!(baz_message.name, "baz");
}
