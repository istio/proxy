use std::env;
use std::fs;
use std::path::PathBuf;

#[test]
fn test_only_binaries() {
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
        "Should have 2 configurations for 2 binaries"
    );

    // Collect configuration names
    let names: Vec<&str> = configurations
        .iter()
        .filter_map(|c| c["name"].as_str())
        .collect();

    // Check that we have configurations for both binaries
    assert!(
        names.contains(&"Debug //only_binaries_test:binary1"),
        "Should have configuration for //only_binaries_test:binary1"
    );
    assert!(
        names.contains(&"Debug //only_binaries_test:binary2"),
        "Should have configuration for //only_binaries_test:binary2"
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

    // Verify no test-related environment variables exist
    for config in configurations {
        let env = &config["env"];
        if let Some(env_obj) = env.as_object() {
            assert!(
                !env_obj.contains_key("BAZEL_TEST"),
                "Binary configurations should not have BAZEL_TEST env var"
            );
            assert!(
                !env_obj.contains_key("TEST_TARGET"),
                "Binary configurations should not have TEST_TARGET env var"
            );
        }
    }
}
