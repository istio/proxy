use std::env;
use std::fs;

pub fn main() {
    let argv1 = env::args().nth(1).expect("Missing output argument");

    fs::write(argv1, "").unwrap();
}
