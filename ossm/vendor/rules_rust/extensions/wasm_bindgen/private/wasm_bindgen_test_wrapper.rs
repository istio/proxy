//! A Bazel wrapper for the wasm-bindgen-test-runner binary.

use std::collections::BTreeMap;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{exit, Command};

use runfiles::{rlocation, Runfiles};

fn write_webdriver_for_browser(
    original: &Path,
    section: Option<&str>,
    args: &Vec<String>,
    browser: &Option<PathBuf>,
    output: &Path,
) {
    // If no section is provided, simply copy the file and early return.
    let section = match section {
        Some(s) => s,
        None => {
            fs::copy(original, output).unwrap_or_else(|e| {
                panic!(
                    "Failed to copy webdriver config: {} -> {}\n{:?}",
                    original.display(),
                    output.display(),
                    e
                );
            });
            return;
        }
    };

    let content = fs::read_to_string(original).unwrap_or_else(|e| {
        panic!(
            "Failed to read webdriver.json at: {}\n{:?}",
            original.display(),
            e
        )
    });

    let mut json_data: serde_json::Value = serde_json::from_str(&content).unwrap_or_else(|e| {
        panic!(
            "Failed to deserialize json at: {}\n{:?}",
            original.display(),
            e
        )
    });

    let options = json_data
        .as_object_mut()
        .unwrap()
        .entry(section)
        .or_insert_with(|| serde_json::json!({}));

    if let Some(binary) = browser {
        options.as_object_mut().unwrap().insert(
            "binary".to_string(),
            serde_json::Value::String(binary.to_string_lossy().to_string()),
        );
    }

    let current_args = options
        .as_object_mut()
        .unwrap()
        .entry("args")
        .or_insert_with(|| serde_json::json!([]));

    for arg in args {
        current_args
            .as_array_mut()
            .unwrap_or_else(|| panic!("Unable to access args array for section `{}`", section))
            .push(serde_json::json!(arg));
    }

    fs::write(
        output,
        serde_json::to_string_pretty(&json_data).expect("Failed to serialize json data"),
    )
    .unwrap_or_else(|e| {
        panic!(
            "Failed to write webdriver config: {}\n{:?}",
            output.display(),
            e
        )
    });
}

fn main() {
    let runfiles = Runfiles::create().expect("Failed to locate runfiles");

    let test_runner = rlocation!(
        runfiles,
        env::var("WASM_BINDGEN_TEST_RUNNER").expect("Failed to find TEST_WASM_BINARY env var")
    )
    .expect("Failed to locate test binary");
    let test_bin = rlocation!(
        runfiles,
        env::var("TEST_WASM_BINARY").expect("Failed to find TEST_WASM_BINARY env var")
    )
    .expect("Failed to locate test binary");

    let browser_type = env::var("BROWSER_TYPE").expect("Failed to find `BROWSER_TYPE` env var");
    let browser = env::var_os("BROWSER").map(|_| {
        rlocation!(runfiles, env::var("BROWSER").unwrap()).expect("Failed to locate browser")
    });

    let webdriver = rlocation!(
        runfiles,
        env::var("WEBDRIVER").expect("Failed to find WEBDRIVER env var.")
    )
    .expect("Failed to locate webdriver");

    let webdriver_json = rlocation!(
        runfiles,
        env::var("WEBDRIVER_JSON").expect("Failed to find WEBDRIVER_JSON env var.")
    )
    .expect("Failed to locate webdriver");

    // Update any existing environment variables.
    let mut env = env::vars().collect::<BTreeMap<_, _>>();
    env.insert("TMP".to_string(), env["TEST_TMPDIR"].clone());
    env.insert("TEMP".to_string(), env["TEST_TMPDIR"].clone());
    env.insert("TMPDIR".to_string(), env["TEST_TMPDIR"].clone());
    env.insert("HOME".to_string(), env["TEST_TMPDIR"].clone());
    env.insert("USERPROFILE".to_string(), env["TEST_TMPDIR"].clone());

    let webdriver_args =
        env::var("WEBDRIVER_ARGS").expect("Failed to find WEBDRIVER_ARGS env var.");

    let undeclared_test_outputs = PathBuf::from(
        env::var("TEST_UNDECLARED_OUTPUTS_DIR")
            .expect("TEST_UNDECLARED_OUTPUTS_DIR should always be defined for tests."),
    );

    let updated_webdriver_json = undeclared_test_outputs.join("webdriver.json");
    env.insert(
        "WASM_BINDGEN_TEST_WEBDRIVER_JSON".to_string(),
        updated_webdriver_json.to_string_lossy().to_string(),
    );

    // Configure the appropriate environment and config values for the browser.
    match browser_type.as_str() {
        "chrome" => {
            env.insert(
                "CHROMEDRIVER".to_string(),
                webdriver.to_string_lossy().to_string(),
            );

            let user_data_dir = undeclared_test_outputs.join("user_data_dir");

            write_webdriver_for_browser(
                &webdriver_json,
                Some("goog:chromeOptions"),
                &vec![format!("user-data-dir={}", user_data_dir.display())],
                &browser,
                &updated_webdriver_json,
            );

            env.insert("CHROMEDRIVER_ARGS".to_string(), webdriver_args);
        }
        "firefox" => {
            env.insert(
                "GECKODRIVER".to_string(),
                webdriver.to_string_lossy().to_string(),
            );

            write_webdriver_for_browser(
                &webdriver_json,
                Some("moz:firefoxOptions"),
                &Vec::new(),
                &browser,
                &updated_webdriver_json,
            );

            // Sandboxing is always disabled as by default tests run in sandboxes anyway
            // and creating an additional one would result in errors.
            env.insert("MOZ_DISABLE_CONTENT_SANDBOX".to_string(), "1".to_string());

            let mut args = Vec::new();

            // Define a clean profile root that can be checked at the end of tests.
            let profile_root = undeclared_test_outputs.join("profile_root");
            fs::create_dir_all(&profile_root).unwrap_or_else(|e| {
                panic!(
                    "Failed to create directory: {}\n{:?}",
                    profile_root.display(),
                    e
                )
            });

            // geckodriver directly accepts the profile root arg so it's tracked here
            // as there's no way to write this info in a usable way to the `webdriver.json`.
            args.push(format!("--profile-root={}", profile_root.display()));

            // geckodriver explicitly accepts a browser flag so we pass that in addition
            // to setting it in the webdriver.json config.
            if let Some(browser) = browser {
                args.push(format!("--binary={}", browser.display()));
            }

            // Ensure logs are always complete.
            args.push("--log-no-truncate".to_string());

            // Some arguments must be passed to geckodriver directly to ensure it's
            // launched in an expected manner
            env.insert(
                "GECKODRIVER_ARGS".to_string(),
                args.join(" ").trim().to_string(),
            );
        }
        "safari" => {
            write_webdriver_for_browser(
                &webdriver_json,
                None,
                &Vec::new(),
                &browser,
                &updated_webdriver_json,
            );

            env.insert("SAFARIDRIVER_ARGS".to_string(), webdriver_args);
        }
        _ => {
            panic!("Unexpected browser type: {}", browser_type)
        }
    }

    // Run the test
    let mut command = Command::new(test_runner);
    command.envs(env).arg(test_bin).args(env::args().skip(1));
    let result = command
        .status()
        .unwrap_or_else(|_| panic!("Failed to spawn command: {:#?}", command));

    if !result.success() {
        exit(
            result
                .code()
                .expect("Completed processes will always have exit codes."),
        )
    }
}
