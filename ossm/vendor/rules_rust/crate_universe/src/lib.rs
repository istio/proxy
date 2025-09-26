#![allow(clippy::large_enum_variant)]

pub mod api;

pub mod cli;

mod config;
mod context;
mod lockfile;
mod metadata;
mod rendering;
mod select;
mod splicing;
mod utils;

#[cfg(test)]
mod test;
