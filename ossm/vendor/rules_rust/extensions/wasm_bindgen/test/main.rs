use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub fn double(i: i32) -> i32 {
    i * 2
}

#[allow(dead_code)]
fn main() {
    println!("Hello {}", double(2));
}

#[cfg(test)]
mod tests {
    use wasm_bindgen_test::*;

    #[wasm_bindgen_test]
    fn test_double_four() {
        assert_eq!(super::double(4), 8);
    }

    #[wasm_bindgen_test(unsupported = test)]
    fn test_double_two() {
        assert_eq!(super::double(2), 4);
    }
}
