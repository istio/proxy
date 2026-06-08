mod helpers;

use helpers::is_fibonacci;
use math_lib::fibonacci;

#[test]
fn fibonacci_test() {
    let fib6 = fibonacci(6);
    assert_eq!(fib6, fibonacci(5) + fibonacci(4));
    assert!(is_fibonacci(fib6));
}
