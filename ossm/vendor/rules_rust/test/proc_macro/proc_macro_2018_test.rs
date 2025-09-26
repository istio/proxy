use proc_macro_2018::make_answer;

make_answer!();

#[test]
fn test_answer_macro() {
    println!("{}", answer());
}
