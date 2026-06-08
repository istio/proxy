use std::env;
use std::fs;
use std::path::PathBuf;

#[test]
fn test_only_tests() {
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

    // Should have exactly 2 configurations
    assert_eq!(
        configurations.len(),
        2,
        "Should have 2 configurations for 2 tests"
    );

    // Collect configuration names
    let names: Vec<&str> = configurations
        .iter()
        .filter_map(|c| c["name"].as_str())
        .collect();

    // Check that we have configurations for both tests
    assert!(
        names.contains(&"Debug //only_tests_test:mylib_test"),
        "Should have configuration for //only_tests_test:mylib_test"
    );
    assert!(
        names.contains(&"Debug //only_tests_test:test"),
        "Should have configuration for //only_tests_test:test"
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

    // Verify test environment variables are present in all configurations
    for config in configurations {
        let env = config["env"]
            .as_object()
            .expect("Test configurations should have env object");

        assert!(
            env.contains_key("BAZEL_TEST"),
            "Test configurations should have BAZEL_TEST env var. Config: {}",
            config["name"]
        );
        assert_eq!(
            env.get("BAZEL_TEST").and_then(|v| v.as_str()),
            Some("1"),
            "BAZEL_TEST should be set to '1'"
        );
        assert!(
            env.contains_key("TEST_TARGET"),
            "Test configurations should have TEST_TARGET env var"
        );
    }
}
