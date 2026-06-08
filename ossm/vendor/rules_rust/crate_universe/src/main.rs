//! The `cargo->bazel` binary's entrypoint

use cargo_bazel::cli;

fn main() -> cli::Result<()> {
    // Parse arguments
    let opt = cli::parse_args();

    let level = match std::env::var("CARGO_BAZEL_DEBUG") {
        Ok(var) => {
            if var == "TRACE" {
                crate::cli::LogLevel::TRACE
            } else {
                crate::cli::LogLevel::DEBUG
            }
        }
        Err(_) => crate::cli::LogLevel::INFO,
    };

    match opt {
        cli::Options::Generate(opt) => {
            cli::init_logging("Generate", level);
            cli::generate(opt)
        }
        cli::Options::Splice(opt) => {
            cli::init_logging("Splice", level);
            cli::splice(opt)
        }
        cli::Options::Query(opt) => {
            cli::init_logging("Query", level);
            cli::query(opt)
        }
        cli::Options::Vendor(opt) => {
            cli::init_logging("Vendor", level);
            cli::vendor(opt)
        }
        cli::Options::Render(opt) => {
            cli::init_logging("Render", level);
            cli::render(opt)
        }
    }
}
