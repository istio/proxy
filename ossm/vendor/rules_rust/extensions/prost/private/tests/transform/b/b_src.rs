// Additional source code for `b.proto`.

pub use greeting::Greeting;

impl Greeting for crate::a::b::B {
    fn greet(&self, name: &str) -> String {
        format!("Hallo, {}, my name is B!", name)
    }
}
