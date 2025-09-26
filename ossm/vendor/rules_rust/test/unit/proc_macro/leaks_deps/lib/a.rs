use my_macro::greet;

pub fn use_macro() -> &'static str {
    greet!()
}
