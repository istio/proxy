use std::os::raw::c_int;

extern "C" {
    pub fn cclinkstampdep() -> c_int;
}

fn main() {
    println!("bin rdep: {}", rdep::rdep());
    println!("cclinkstampdep: {}", unsafe { cclinkstampdep() });
}
