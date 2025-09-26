// This crate depends on 2 crates with one of them depending on the other one.
// If the crates are not set correctly in the dependency chain, this crate won't
// compile. The order of the `extern crate` is important to trigger that bug.
extern crate mod1;
extern crate mod2;

pub fn greet(name: &str) {
    println!("{}", mod2::greeter(name))
}

pub fn greet_default() {
    println!("{}", mod2::default_greeter())
}

/// This is a documentation.
///
/// # Examples
///
/// ```rust
/// assert!(
///   mod3::am_i_the_world("world") == true
/// );
/// assert!(
///   mod3::am_i_the_world("myself") == false
/// );
/// ```
pub fn am_i_the_world(me: &str) -> bool {
    me == mod1::world()
}

#[cfg(test)]
mod test {
    #[test]
    fn test_am_i_the_world() {
        assert!(super::am_i_the_world("world"));
        assert!(!super::am_i_the_world("bob"));
    }
}
