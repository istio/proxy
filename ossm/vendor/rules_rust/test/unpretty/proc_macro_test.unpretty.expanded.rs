#![feature(prelude_import)]
#[prelude_import]
use std::prelude::rust_2021::*;
#[macro_use]
extern crate std;
use proc_macro::make_answer;

fn answer() -> u32 { 42 }

