pub fn greeting_from(from: &str) -> String {
    format!("Hello from {from}!")
}

pub fn greeting_a() -> String {
    greeting_from("lib_a")
}
