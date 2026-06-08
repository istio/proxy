//! Command line interface entry points and utilities

mod generate;
mod query;
mod render;
mod splice;
mod vendor;

use clap::Parser;
use tracing::Subscriber;
use tracing_subscriber::fmt::format::{Format, Full};
use tracing_subscriber::fmt::time::SystemTime;
use tracing_subscriber::fmt::{FormatEvent, FormatFields};
use tracing_subscriber::registry::LookupSpan;
use tracing_subscriber::FmtSubscriber;

pub use tracing::Level as LogLevel;

pub use self::generate::GenerateOptions;
pub use self::query::QueryOptions;
pub use self::render::RenderOptions;
pub use self::splice::SpliceOptions;
pub use self::vendor::VendorOptions;

// Entrypoints
pub use generate::generate;
pub use query::query;
pub use render::render;
pub use splice::splice;
pub use vendor::vendor;

#[derive(Parser, Debug)]
#[clap(
    name = "cargo-bazel",
    about = "crate_universe` is a collection of tools which use Cargo to generate build targets for Bazel.",
    version
)]
pub enum Options {
    /// Generate Bazel Build files from a Cargo manifest.
    Generate(GenerateOptions),

    /// Splice together disjoint Cargo and Bazel info into a single Cargo workspace manifest.
    Splice(SpliceOptions),

    /// Query workspace info to determine whether or not a repin is needed.
    Query(QueryOptions),

    /// Vendor BUILD files to the workspace with either repository definitions or `cargo vendor` generated sources.
    Vendor(VendorOptions),

    /// Render a BUILD file for a single crate.
    Render(RenderOptions),
}

// Convenience wrappers to avoid dependencies in the binary
pub type Result<T> = anyhow::Result<T>;

pub fn parse_args() -> Options {
    Options::parse()
}

const EXPECTED_LOGGER_NAMES: [&str; 5] = ["Generate", "Splice", "Query", "Vendor", "Render"];

/// A wrapper for the tracing-subscriber default [FormatEvent]
/// that prepends the name of the active CLI option.
struct LoggingFormatEvent {
    name: String,
    base: Format<Full, SystemTime>,
}

impl<S, N> FormatEvent<S, N> for LoggingFormatEvent
where
    S: Subscriber + for<'a> LookupSpan<'a>,
    N: for<'a> FormatFields<'a> + 'static,
{
    fn format_event(
        &self,
        ctx: &tracing_subscriber::fmt::FmtContext<'_, S, N>,
        mut writer: tracing_subscriber::fmt::format::Writer<'_>,
        event: &tracing::Event<'_>,
    ) -> std::fmt::Result {
        write!(writer, "{} ", self.name)?;
        self.base.format_event(ctx, writer, event)
    }
}

impl LoggingFormatEvent {
    fn new(name: &str) -> Self {
        Self {
            name: name.to_owned(),
            base: Format::default(),
        }
    }
}

/// Initialize logging for one of the cli options.
pub fn init_logging(name: &str, level: LogLevel) {
    if !EXPECTED_LOGGER_NAMES.contains(&name) {
        panic!(
            "Unexpected logger name {}, use of one of {:?}",
            name, EXPECTED_LOGGER_NAMES
        );
    }

    let subscriber = FmtSubscriber::builder()
        .with_max_level(level)
        .event_format(LoggingFormatEvent::new(name))
        .finish();

    tracing::subscriber::set_global_default(subscriber).expect("setting default subscriber failed");
}
