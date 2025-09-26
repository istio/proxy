extern "C" {
    fn foo() -> i32;
}
fn main() {
    println!("{}", unsafe { foo() })
}
