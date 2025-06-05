#[test]
pub fn test_tool_exec() {
    let tool_path = env!("TOOL_PATH");
    assert!(
        tool_path.contains("-exec-"),
        "tool_path did not contain '-exec-'",
    );
}
