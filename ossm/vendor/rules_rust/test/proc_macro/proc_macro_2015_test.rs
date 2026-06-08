extern crate proc_macro_2015;
use proc_macro_2015::make_answer;

make_answer!();

fn main() {
    println!("{}", answer());
}
