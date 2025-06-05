// Copyright 2015 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

    /// Constructs a new `Greeter` with greeting defined in txt file
    ///
    /// # Examples
    ///
    /// ```
    /// use hello_lib::greeter::Greeter;
    ///
    /// let greeter = Greeter::from_txt_file()?;
    /// ```
    pub fn from_txt_file() -> runfiles::Result<Greeter> {
        let r = runfiles::Runfiles::create()?;
        Ok(Greeter {
            greeting: std::fs::read_to_string(
                runfiles::rlocation!(r, "rules_rust/test/rust/greeting.txt").unwrap(),
            )
            .map_err(runfiles::RunfilesError::RunfileIoError)?,
        })
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

    #[test]
    fn test_greeting_from_txt_file() {
        let welcome = Greeter::from_txt_file().unwrap();
        assert_eq!("Welcome Rust", welcome.greeting("Rust"));
    }
}
