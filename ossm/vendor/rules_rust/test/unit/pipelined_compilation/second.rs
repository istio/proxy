use first::first_fun;
use my_macro::noop;

#[noop]
pub fn fun() {
    println!("{}", first_fun())
}
