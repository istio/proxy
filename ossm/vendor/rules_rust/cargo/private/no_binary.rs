//! A cross platform implementation of `/bin/false`

fn main() {
    let program_name = std::env::args()
        .next()
        .unwrap_or_else(|| "unknown".to_string());

    eprintln!("No binary provided for {}", program_name);
    std::process::exit(1);
}
