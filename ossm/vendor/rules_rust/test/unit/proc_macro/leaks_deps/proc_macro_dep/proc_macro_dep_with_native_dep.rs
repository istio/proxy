extern "C" {
    pub fn forty_two_from_cc() -> i32;
}

pub fn forty_two() -> i32 {
    unsafe { forty_two_from_cc() }
}
