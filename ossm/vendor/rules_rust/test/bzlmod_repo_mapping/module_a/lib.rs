#[cfg(test)]
mod test {
    #[test]
    fn test_repo_remapping() {
        let data = lib_b::read_file_from_module_c();
        assert_eq!(data, "module(name = \"module_c\")\n");
    }
}
