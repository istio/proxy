#![feature(prelude_import)]
extern crate std;
#[prelude_import]
use std::prelude::rust_2021::*;
use proc_macro::make_answer;

fn answer() -> u32 { 42 }

