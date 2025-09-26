//! Test outputs of `@rules_rust_mdbook//tests/generated_srcs/...`

use runfiles::{rlocation, Runfiles};
use std::fs;

#[test]
fn test_generated_output() {
    let r = Runfiles::create().unwrap();

    let dir = rlocation!(r, env!("MDBOOK_OUTPUT_RLOCATIONPATH")).unwrap();

    let chapter_1 = dir.join("chapter_1.html");
    let content = fs::read_to_string(chapter_1).unwrap();
    assert!(content.contains("La-Li-Lu-Le-Lo"));
}
