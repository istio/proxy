/// Object that displays a greeting.
pub struct Greeter {
    greeting: String,
}

/// Implementation of Greeter.
impl Greeter {
    /// Constructs a new `Greeter`.
    ///
    /// # Examples
    ///
    /// ```
    /// use hello_lib::greeter::Greeter;
    ///
    /// let greeter = Greeter::new("Hello");
    /// ```
    pub fn new(greeting: &str) -> Greeter {
        Greeter {
            greeting: greeting.to_string(),
        }
    }

    /// Returns the greeting as a string.
    ///
    /// # Examples
    ///
    /// ```
    /// use hello_lib::greeter::Greeter;
    ///
    /// let greeter = Greeter::new("Hello");
    /// let greeting = greeter.greeting("World");
    /// ```
    pub fn greeting(&self, thing: &str) -> String {
        format!("{} {}", &self.greeting, thing)
    }

    /// Prints the greeting.
    ///
    /// # Examples
    ///
    /// ```
    /// use hello_lib::greeter::Greeter;
    ///
    /// let greeter = Greeter::new("Hello");
    /// greeter.greet("World");
    /// ```
    pub fn greet(&self, thing: &str) {
        println!("{} {}", &self.greeting, thing);
    }
}

#[cfg(test)]
mod test {
    use super::Greeter;

    #[test]
    fn test_greeting() {
        let hello = Greeter::new("Hi");
        assert_eq!("Hi Rust", hello.greeting("Rust"));
    }
}
