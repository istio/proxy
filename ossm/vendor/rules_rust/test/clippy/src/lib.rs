pub fn greeting() -> String {
    "Hello World".to_owned()
}

// too_many_args/clippy.toml will require no more than 2 args.
pub fn with_args(_: u32, _: u32, _: u32) {}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn it_works() {
        assert_eq!(greeting(), "Hello World".to_owned());
    }
}
