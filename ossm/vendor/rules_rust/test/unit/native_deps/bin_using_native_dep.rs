extern "C" {
    fn native_dep() -> isize;
}
fn main() {
    println!("{}", unsafe { native_dep() })
}
