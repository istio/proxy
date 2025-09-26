use std::path::Path;

pub fn ensure_helper_data_exists() {
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("helper_data.txt");
    assert!(path.exists(), "not found: {}", path.display());
}
