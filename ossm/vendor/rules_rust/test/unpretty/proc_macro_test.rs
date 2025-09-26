use proc_macro::make_answer;

make_answer!();

#[test]
fn test_answer_macro() {
    println!("{}", answer());
}
