extern "C" {
    fn native_dep() -> isize;
}
pub fn use_native_dep() {
    println!("{}", unsafe { native_dep() })
}
