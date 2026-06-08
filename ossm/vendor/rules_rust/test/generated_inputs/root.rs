mod generated_in_different_cfg;

pub fn use_generated_fn() -> String {
    "Using generated function: ".to_owned() + &generated_in_different_cfg::generated_fn() 
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_use_generated_fn() {
        assert_eq!(super::use_generated_fn(), "Using generated function: Generated".to_owned());
    }
}
