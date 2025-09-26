/// A function that adds two numbers
pub fn add(a: i32, b: i32) -> i32 {
    // This line adds two i32's
    a + b
}

/// Calculate the n'th fibonacci number
pub fn fibonacci(n: i32) -> i32 {
    match n {
        0 => 1,
        1 => 1,
        _ => fibonacci(n - 1) + fibonacci(n - 2),
    }
}
