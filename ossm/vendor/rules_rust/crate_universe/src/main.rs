//! The `cargo->bazel` binary's entrypoint

use cargo_bazel::cli;

fn main() -> cli::Result<()> {
    // Parse arguments
    let opt = cli::parse_args();

    let verbose_logging = std::env::var("CARGO_BAZEL_DEBUG").is_ok();

    match opt {
        cli::Options::Generate(opt) => {
            cli::init_logging("Generate", verbose_logging);
            cli::generate(opt)
        }
        cli::Options::Splice(opt) => {
            cli::init_logging("Splice", verbose_logging);
            cli::splice(opt)
        }
        cli::Options::Query(opt) => {
            cli::init_logging("Query", verbose_logging);
            cli::query(opt)
        }
        cli::Options::Vendor(opt) => {
            cli::init_logging("Vendor", verbose_logging);
            cli::vendor(opt)
        }
        cli::Options::Render(opt) => {
            cli::init_logging("Render", verbose_logging);
            cli::render(opt)
        }
    }
}
