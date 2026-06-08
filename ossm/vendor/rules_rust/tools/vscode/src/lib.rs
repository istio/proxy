use std::collections::BTreeMap;
use std::fs;
use std::process::Command;

use anyhow::{bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use log::debug;
use serde::Serialize;
use serde_json::Value;

pub struct BazelInfo {
    pub output_base: String,
    pub workspace: String,
}

fn bazel_command(bazel: &Utf8Path, workspace_root: &Utf8Path) -> Command {
    let mut cmd = Command::new(bazel);
    cmd.current_dir(workspace_root);
    cmd
}

impl BazelInfo {
    pub fn new(output_base: String, workspace: String) -> Self {
        Self {
            output_base,
            workspace,
        }
    }

    pub fn try_new(bazel: &Utf8Path, workspace_root: &Utf8Path) -> anyhow::Result<Self> {
        let output = bazel_command(bazel, workspace_root)
            .arg("info")
            .output()
            .context("Failed to execute 'bazel info'")?;

        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            bail!("bazel info failed: {}", stderr);
        }

        let info_map: BTreeMap<String, String> = String::from_utf8(output.stdout)?
            .trim()
            .lines()
            .filter_map(|line| line.split_once(':'))
            .map(|(k, v)| (k.to_owned(), v.trim().to_owned()))
            .collect();

        Ok(Self {
            output_base: info_map
                .get("output_base")
                .context("Failed to query `bazel info output_base`")?
                .clone(),
            workspace: info_map
                .get("workspace")
                .context("Failed to query `bazel info workspace`")?
                .clone(),
        })
    }
}

/// Information about a Bazel target for debugging.
#[derive(Debug, Clone)]
pub struct TargetInfo {
    pub label: String,
    pub binary_path: Utf8PathBuf,
    pub is_test: bool,
    pub target_kind: String,
}

/// VSCode launch configuration for debugging.
#[derive(Debug, Serialize)]
pub struct LaunchConfig {
    pub name: String,
    pub r#type: String,
    pub request: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub program: Option<String>,
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub args: Vec<String>,
    pub cwd: String,
    #[serde(rename = "sourceLanguages")]
    pub source_languages: Vec<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub env: Option<BTreeMap<String, String>>,
    #[serde(
        rename = "targetCreateCommands",
        skip_serializing_if = "Option::is_none"
    )]
    pub target_create_commands: Option<Vec<String>>,
}

/// VSCode task configuration for building.
#[derive(Debug, Serialize)]
pub struct TaskConfig {
    pub label: String,
    pub r#type: String,
    pub command: String,
    pub args: Vec<String>,
    pub group: String,
    pub presentation: TaskPresentation,
    #[serde(rename = "problemMatcher")]
    pub problem_matcher: Vec<String>,
}

#[derive(Debug, Serialize)]
pub struct TaskPresentation {
    pub reveal: String,
    pub panel: String,
    #[serde(rename = "showReuseMessage")]
    pub show_reuse_message: bool,
    pub clear: bool,
}

/// Generator for VSCode launch configurations.
pub struct LaunchConfigGenerator {
    workspace_root: Utf8PathBuf,
    bazel_binary: Utf8PathBuf,
    bazel_info: BazelInfo,
}

impl LaunchConfigGenerator {
    pub fn new(workspace_root: Utf8PathBuf, bazel_info: BazelInfo) -> Self {
        Self {
            workspace_root,
            bazel_binary: "bazel".into(),
            bazel_info,
        }
    }

    pub fn with_bazel_binary(mut self, bazel: Utf8PathBuf) -> Self {
        self.bazel_binary = bazel;
        self
    }

    /// Query information about multiple targets at once.
    pub fn query_targets_batch(&mut self, targets: &[String]) -> Result<Vec<TargetInfo>> {
        if targets.is_empty() {
            return Ok(vec![]);
        }

        // Get all target kinds in one query
        let target_kinds = self.batch_query_target_kinds(targets)?;

        let mut results = Vec::new();
        for target in targets {
            if let Some(target_kind) = target_kinds.get(target) {
                let is_test = target_kind.contains("rust_test");

                // We don't need to resolve binary paths now - that will happen at debug time
                // Just create a placeholder path that will be resolved by the pre-launch task
                results.push(TargetInfo {
                    label: target.to_string(),
                    binary_path: Utf8PathBuf::from(""), // Placeholder - resolved at debug time
                    is_test,
                    target_kind: target_kind.clone(),
                });
            }
        }

        Ok(results)
    }

    /// Batch query target kinds for multiple targets at once.
    fn batch_query_target_kinds(&self, targets: &[String]) -> Result<BTreeMap<String, String>> {
        let target_pattern = targets.join(" + ");

        let output = bazel_command(&self.bazel_binary, &self.workspace_root)
            .arg("query")
            .arg("--output=label_kind")
            .arg(&target_pattern)
            .output()
            .context("Failed to execute 'bazel query'")?;

        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            bail!("bazel query failed for targets: {}", stderr);
        }

        let stdout = String::from_utf8(output.stdout)?;
        let mut result = BTreeMap::new();

        for line in stdout.trim().lines() {
            // Format: "rust_test rule //path/to:target"
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.len() >= 3 {
                let kind = parts[0];
                let label = parts[2];

                if !kind.starts_with("rust_") {
                    // Skip non-Rust targets
                    continue;
                }

                result.insert(label.to_string(), kind.to_string());
            }
        }

        Ok(result)
    }

    /// Execute a Bazel query and return the results.
    fn execute_query(&self, query_expr: &str) -> Result<Vec<String>> {
        debug!("Executing Bazel query: {}", query_expr);

        let output = bazel_command(&self.bazel_binary, &self.workspace_root)
            .arg("query")
            .arg(query_expr)
            .output()
            .context("Failed to execute 'bazel query'")?;

        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            bail!("bazel query failed for '{}': {}", query_expr, stderr);
        }

        let targets = String::from_utf8(output.stdout)?
            .lines()
            .map(|line| line.trim().to_string())
            .filter(|line| !line.is_empty())
            .collect::<Vec<_>>();

        debug!("Query returned {} targets", targets.len());
        Ok(targets)
    }

    /// Find Rust targets using query patterns (similar to gen_rust_project approach).
    /// Returns TargetInfo for all rust_binary and rust_test targets found.
    pub fn find_rust_targets(&self, patterns: &[String]) -> Result<Vec<TargetInfo>> {
        if patterns.is_empty() {
            return Ok(vec![]);
        }

        let target_pattern = patterns.join(" + ");
        let mut results = Vec::new();

        // Query rust_binary targets
        let binary_query = format!("kind('rust_binary', {})", target_pattern);
        let binary_targets = self.execute_query(&binary_query)?;
        for label in binary_targets {
            results.push(TargetInfo {
                label,
                binary_path: Utf8PathBuf::from(""), // Placeholder - resolved at debug time
                is_test: false,
                target_kind: "rust_binary".to_string(),
            });
        }

        // Query rust_test targets
        let test_query = format!("kind('rust_test', {})", target_pattern);
        let test_targets = self.execute_query(&test_query)?;
        for label in test_targets {
            results.push(TargetInfo {
                label,
                binary_path: Utf8PathBuf::from(""), // Placeholder - resolved at debug time
                is_test: true,
                target_kind: "rust_test".to_string(),
            });
        }

        Ok(results)
    }

    /// Generate a launch configuration for a target.
    pub fn generate_launch_config(&self, target_info: &TargetInfo) -> Result<LaunchConfig> {
        let name = format!("Debug {}", target_info.label);

        // Use CodeLLDB's "custom" request with Python scripting to build and get the binary path
        // This consolidates everything into targetCreateCommands, no separate task needed
        let target_create_commands = vec![
            // Multi-line Python script that:
            // 1. Runs bazel with --run_under to get the binary path
            // 2. Parses stderr for the bazel-out path
            // 3. Creates the debug target
                "script ".to_owned() + &[
                    "import subprocess, os, sys".to_owned(),
                    format!("result = subprocess.run(['bazel', 'run', '--compilation_mode=dbg', '--strip=never', '--run_under=@rules_rust//tools/vscode:get_binary_path', '{}'], stdout=subprocess.PIPE, text=True, cwd='${{workspaceFolder}}')", target_info.label),
                    "binary_path = result.stdout.strip().splitlines()[-1]".to_owned(),
                    format!("assert binary_path, 'No binary path output for {}'", target_info.label),
                    "abs_path = os.path.join('${workspaceFolder}', binary_path)".to_owned(),
                    "lldb.debugger.CreateTarget(abs_path)".to_owned(),
                ].join("; ")
        ];

        let mut config = LaunchConfig {
            name,
            r#type: "lldb".to_string(),
            request: "custom".to_string(),
            program: None,
            args: vec![],
            cwd: self.workspace_root.to_string(),
            source_languages: vec!["rust".to_string()],
            env: None,
            target_create_commands: Some(target_create_commands),
        };

        // Add test environment if this is a test target
        if target_info.is_test {
            let test_env = self.generate_test_environment(&target_info.label);
            config.env = Some(test_env);
        }

        Ok(config)
    }

    /// Sanitize target name for use in filenames.
    fn sanitize_target_name(&self, target: &str) -> String {
        target.replace("//", "").replace([':', '/'], "_")
    }

    /// Generate test environment variables based on Bazel test encyclopedia.
    fn generate_test_environment(&self, target: &str) -> BTreeMap<String, String> {
        let workspace_name = &self.bazel_info.workspace;

        let mut env = BTreeMap::new();

        let sanitized_name = self.sanitize_target_name(target);
        let vscode_dir = self.workspace_root.join(".vscode");

        // Core test environment variables from Bazel test encyclopedia
        // https://bazel.build/reference/test-encyclopedia
        env.insert("BAZEL_TEST".to_string(), "1".to_string());
        env.insert("TEST_TARGET".to_string(), target.to_string());
        env.insert("TEST_WORKSPACE".to_string(), workspace_name.clone());

        // Test output directories (use .vscode subdirectories)
        env.insert(
            "TEST_TMPDIR".to_string(),
            vscode_dir
                .join(format!("bazel-test-tmp-{}", sanitized_name))
                .to_string(),
        );
        env.insert(
            "TEST_UNDECLARED_OUTPUTS_DIR".to_string(),
            vscode_dir
                .join(format!("bazel-test-outputs-{}", sanitized_name))
                .to_string(),
        );
        env.insert(
            "TEST_UNDECLARED_OUTPUTS_ANNOTATIONS_DIR".to_string(),
            vscode_dir
                .join(format!("bazel-test-annotations-{}", sanitized_name))
                .to_string(),
        );

        // Rust-specific
        env.insert("RUST_BACKTRACE".to_string(), "all".to_string());

        env
    }

    /// Read existing launch.json file if it exists.
    pub fn read_existing_launch_config(&self, path: &Utf8Path) -> Result<Option<Value>> {
        if !path.exists() {
            return Ok(None);
        }

        let content = fs::read_to_string(path)
            .with_context(|| format!("Failed to read existing launch config from {}", path))?;

        let config: Value = serde_json::from_str(&content)
            .with_context(|| format!("Failed to parse existing launch config from {}", path))?;

        Ok(Some(config))
    }

    /// Read existing tasks.json file if it exists.
    pub fn read_existing_tasks_config(&self, path: &Utf8Path) -> Result<Option<Value>> {
        if !path.exists() {
            return Ok(None);
        }

        let content = fs::read_to_string(path)
            .with_context(|| format!("Failed to read existing tasks config from {}", path))?;

        let config: Value = serde_json::from_str(&content)
            .with_context(|| format!("Failed to parse existing tasks config from {}", path))?;

        Ok(Some(config))
    }

    /// Check if a configuration name matches our "Debug {label}" pattern and extract the label.
    fn extract_debug_label(name: &str) -> Option<&str> {
        name.strip_prefix("Debug ")
    }

    /// Check if a task name matches our "bazel-debug: build {label}" pattern and extract the label.
    fn extract_build_task_label(name: &str) -> Option<&str> {
        name.strip_prefix("bazel-debug: build ")
    }

    /// Filter out existing configurations that match our generated patterns.
    fn filter_existing_configurations(
        existing: &mut Value,
        generated_targets: &[String],
    ) -> Result<()> {
        if let Some(configurations) = existing.get_mut("configurations") {
            if let Some(configs_array) = configurations.as_array_mut() {
                configs_array.retain(|config| {
                    if let Some(name) = config.get("name").and_then(|v| v.as_str()) {
                        if let Some(label) = Self::extract_debug_label(name) {
                            // Check if this is a valid Bazel label that we're generating
                            if label::analyze(label).is_ok() {
                                // If it's in our list of targets to generate, remove it
                                return !generated_targets.contains(&label.to_string());
                            }
                        }
                    }
                    // Keep configurations that don't match our pattern
                    true
                });
            }
        }
        Ok(())
    }

    /// Filter out existing tasks that match our generated patterns.
    fn filter_existing_tasks(existing: &mut Value, generated_targets: &[String]) -> Result<()> {
        if let Some(tasks) = existing.get_mut("tasks") {
            if let Some(tasks_array) = tasks.as_array_mut() {
                tasks_array.retain(|task| {
                    if let Some(label) = task.get("label").and_then(|v| v.as_str()) {
                        if let Some(target_label) = Self::extract_build_task_label(label) {
                            // Check if this is a valid Bazel label that we're generating
                            if label::analyze(target_label).is_ok() {
                                // If it's in our list of targets to generate, remove it
                                return !generated_targets.contains(&target_label.to_string());
                            }
                        }
                    }
                    // Keep tasks that don't match our pattern
                    true
                });
            }
        }
        Ok(())
    }

    /// Merge new configurations with existing launch.json.
    pub fn merge_launch_configs(
        &self,
        new_configs: &[LaunchConfig],
        existing_path: &Utf8Path,
    ) -> Result<Value> {
        let generated_targets: Vec<String> = new_configs
            .iter()
            .filter_map(|config| Self::extract_debug_label(&config.name))
            .map(|s| s.to_string())
            .collect();

        let mut result =
            if let Some(mut existing) = self.read_existing_launch_config(existing_path)? {
                // Remove existing configurations for targets we're regenerating
                Self::filter_existing_configurations(&mut existing, &generated_targets)?;
                existing
            } else {
                // Create new launch.json structure
                serde_json::json!({
                    "version": "0.2.0",
                    "configurations": []
                })
            };

        // Add new configurations
        if let Some(configurations) = result.get_mut("configurations") {
            if let Some(configs_array) = configurations.as_array_mut() {
                for config in new_configs {
                    configs_array.push(serde_json::to_value(config)?);
                }
            }
        }

        Ok(result)
    }

    /// Merge new tasks with existing tasks.json.
    pub fn merge_tasks_configs(
        &self,
        new_tasks: &[TaskConfig],
        existing_path: &Utf8Path,
    ) -> Result<Value> {
        let generated_targets: Vec<String> = new_tasks
            .iter()
            .filter_map(|task| Self::extract_build_task_label(&task.label))
            .map(|s| s.to_string())
            .collect();

        let mut result =
            if let Some(mut existing) = self.read_existing_tasks_config(existing_path)? {
                // Remove existing tasks for targets we're regenerating
                Self::filter_existing_tasks(&mut existing, &generated_targets)?;
                existing
            } else {
                // Create new tasks.json structure
                serde_json::json!({
                    "version": "2.0.0",
                    "tasks": []
                })
            };

        // Add new tasks
        if let Some(tasks) = result.get_mut("tasks") {
            if let Some(tasks_array) = tasks.as_array_mut() {
                for task in new_tasks {
                    tasks_array.push(serde_json::to_value(task)?);
                }
            }
        }

        Ok(result)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_target_info_creation() {
        let target_info = TargetInfo {
            label: "//test:my_test".to_string(),
            binary_path: "/path/to/binary".into(),
            is_test: true,
            target_kind: "rust_test rule".to_string(),
        };

        assert_eq!(target_info.label, "//test:my_test");
        assert!(target_info.is_test);
    }
}
