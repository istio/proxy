use std::env;
use std::fs;
use std::path::PathBuf;

#[test]
fn test_binaries_and_tests() {
    let launch_json_path = PathBuf::from(env::var("LAUNCH_JSON").unwrap());
    let content = fs::read_to_string(&launch_json_path)
        .unwrap_or_else(|_| panic!("couldn't open {:?}", &launch_json_path));

    let launch: serde_json::Value = serde_json::from_str(&content)
        .unwrap_or_else(|e| panic!("Failed to parse launch.json: {}", e));

    // Verify version
    assert_eq!(
        launch["version"].as_str(),
        Some("0.2.0"),
        "Should have version 0.2.0"
    );

    // Get configurations array
    let configurations = launch["configurations"]
        .as_array()
        .expect("configurations should be an array");

    // Should have exactly 3 configurations (1 binary + 2 tests)
    assert_eq!(
        configurations.len(),
        3,
        "Should have 3 configurations (1 binary + 2 tests)"
    );

    // Collect configuration names
    let names: Vec<&str> = configurations
        .iter()
        .filter_map(|c| c["name"].as_str())
        .collect();

    // Check that we have configuration for the binary
    assert!(
        names.contains(&"Debug //binaries_and_tests_test:main_binary"),
        "Should have configuration for //binaries_and_tests_test:main_binary"
    );

    // Check that we have configurations for both tests
    assert!(
        names.contains(&"Debug //binaries_and_tests_test:mylib_test"),
        "Should have configuration for //binaries_and_tests_test:mylib_test"
    );
    assert!(
        names.contains(&"Debug //binaries_and_tests_test:test"),
        "Should have configuration for //binaries_and_tests_test:test"
    );

    // All configurations should be lldb type with custom request
    for config in configurations {
        assert_eq!(
            config["type"].as_str(),
            Some("lldb"),
            "All configurations should be lldb type"
        );
        assert_eq!(
            config["request"].as_str(),
            Some("custom"),
            "All configurations should have request=custom"
        );

        // Check sourceLanguages contains rust
        let source_languages = config["sourceLanguages"]
            .as_array()
            .expect("sourceLanguages should be an array");
        let has_rust = source_languages
            .iter()
            .any(|lang| lang.as_str() == Some("rust"));
        assert!(has_rust, "Should have rust in sourceLanguages");
    }

    // Count test configurations (should have BAZEL_TEST) vs binary configurations
    let test_configs: Vec<&serde_json::Value> = configurations
        .iter()
        .filter(|c| {
            c["env"]
                .as_object()
                .map(|env| env.contains_key("BAZEL_TEST"))
                .unwrap_or(false)
        })
        .collect();

    let binary_configs: Vec<&serde_json::Value> = configurations
        .iter()
        .filter(|c| {
            !c["env"]
                .as_object()
                .map(|env| env.contains_key("BAZEL_TEST"))
                .unwrap_or(false)
        })
        .collect();

    assert_eq!(test_configs.len(), 2, "Should have 2 test configurations");
    assert_eq!(
        binary_configs.len(),
        1,
        "Should have 1 binary configuration"
    );

    // Verify test configurations have required env vars
    for config in &test_configs {
        let env = config["env"]
            .as_object()
            .expect("Test configuration should have env object");

        assert_eq!(
            env.get("BAZEL_TEST").and_then(|v| v.as_str()),
            Some("1"),
            "Test BAZEL_TEST should be set to '1'"
        );
        assert!(
            env.contains_key("TEST_TARGET"),
            "Test configurations should have TEST_TARGET env var"
        );
    }

    // Verify binary configuration does not have test env vars
    for config in &binary_configs {
        let env = &config["env"];
        if let Some(env_obj) = env.as_object() {
            assert!(
                !env_obj.contains_key("BAZEL_TEST"),
                "Binary configuration should not have BAZEL_TEST"
            );
            assert!(
                !env_obj.contains_key("TEST_TARGET"),
                "Binary configuration should not have TEST_TARGET"
            );
        }
    }
}
