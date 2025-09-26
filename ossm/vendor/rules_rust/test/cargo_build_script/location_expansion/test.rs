#[test]
pub fn test_data_rootpath() {
    assert_eq!(
        "test/cargo_build_script/location_expansion/target_data.txt",
        env!("DATA_ROOTPATH")
    );
}

#[test]
pub fn test_data_rlocation() {
    assert!(
        [
            // workspace
            "rules_rust/test/cargo_build_script/location_expansion/target_data.txt",
            // bzlmod
            "_main/test/cargo_build_script/location_expansion/target_data.txt",
        ]
        .contains(&env!("DATA_RLOCATIONPATH")),
        concat!("Unexpected rlocationpath: ", env!("DATA_RLOCATIONPATH"))
    );
}

#[test]
pub fn test_tool_rootpath() {
    assert_eq!(
        "test/cargo_build_script/location_expansion/exec_data.txt",
        env!("TOOL_ROOTPATH")
    );
}

#[test]
pub fn test_tool_rlocationpath() {
    assert!(
        [
            // workspace
            "rules_rust/test/cargo_build_script/location_expansion/exec_data.txt",
            // bzlmod
            "_main/test/cargo_build_script/location_expansion/exec_data.txt",
        ]
        .contains(&env!("TOOL_RLOCATIONPATH")),
        concat!("Unexpected rlocationpath: ", env!("TOOL_RLOCATIONPATH"))
    );
}

#[test]
pub fn test_execpath() {
    // Replace `\` to ensure paths are consistent on Windows.`
    let data_execpath = env!("DATA_EXECPATH").replace('\\', "/");
    let tool_execpath = env!("TOOL_EXECPATH").replace('\\', "/");

    let data_path = data_execpath
        .split_at(
            data_execpath
                .find("/bazel-out/")
                .unwrap_or_else(|| panic!("Failed to parse execroot from: {}", data_execpath)),
        )
        .1;
    let tool_path = tool_execpath
        .split_at(
            tool_execpath
                .find("/bazel-out/")
                .unwrap_or_else(|| panic!("Failed to parse execroot from: {}", tool_execpath)),
        )
        .1;

    let (data_cfg, data_short_path) = data_path.split_at(
        data_path
            .find("/bin/")
            .unwrap_or_else(|| panic!("Failed to find bin in {}", data_path))
            + "/bin/".len(),
    );
    let (tool_cfg, tool_short_path) = tool_path.split_at(
        tool_path
            .find("/bin/")
            .unwrap_or_else(|| panic!("Failed to find bin in {}", tool_path))
            + "/bin/".len(),
    );

    assert_ne!(
        data_cfg, tool_cfg,
        "Data and tools should not be from the same configuration."
    );

    assert_eq!(
        data_short_path,
        "test/cargo_build_script/location_expansion/target_data.txt"
    );
    assert_eq!(
        tool_short_path,
        "test/cargo_build_script/location_expansion/exec_data.txt"
    );
}
