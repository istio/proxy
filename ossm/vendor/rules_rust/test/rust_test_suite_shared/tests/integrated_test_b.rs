mod helpers;

use helpers::is_fibonacci;
use math_lib::fibonacci;

#[test]
fn fibonacci_test() {
    let fib7 = fibonacci(7);
    assert_eq!(fib7, fibonacci(6) + fibonacci(5));
    assert!(is_fibonacci(fib7));
}
