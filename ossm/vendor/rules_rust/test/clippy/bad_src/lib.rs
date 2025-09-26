pub fn foobar() {
    assert!(true);
    loop {
        println!("{}", "Hello World");
        break;
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_works() {
        assert!(true);
        loop {
            println!("{}", "Hello World");
            break;
        }
    }
}
