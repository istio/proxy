// Binet's formula for checking Fibonacci numbers
pub fn is_fibonacci(x: i32) -> bool {
    is_perfect_square(5 * x * x + 4) || is_perfect_square(5 * x * x - 4)
}

fn is_perfect_square(x: i32) -> bool {
    if x < 0 {
        return false;
    }
    let y = (x as f64).sqrt() as i32;
    y * y == x
}
