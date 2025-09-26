//! Implement a trait which is used to generate greetings.

pub trait Greeting {
    fn greet(&self, name: &str) -> String;
}
