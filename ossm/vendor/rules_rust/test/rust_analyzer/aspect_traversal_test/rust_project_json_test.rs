#[cfg(test)]
mod tests {
    use serde::Deserialize;

    use std::collections::BTreeSet;
    use std::env;
    use std::path::PathBuf;

    #[allow(clippy::uninlined_format_args)]
    #[test]
    fn test_aspect_traverses_all_the_right_corners_of_target_graph() {
        let rust_project_path = PathBuf::from(env::var("RUST_PROJECT_JSON").unwrap());

        let content = std::fs::read_to_string(&rust_project_path)
            .unwrap_or_else(|_| panic!("couldn't open {:?}", &rust_project_path));

        for dep in &[
            "lib_dep",
            "actual_dep",
            "dep_of_aliased_dep",
            "custom_actual_dep",
            "dep_of_custom_aliased_dep",
            "extra_test_dep",
            "proc_macro_dep",
            "extra_proc_macro_dep",
        ] {
            assert!(
                content.contains(dep),
                "expected rust-project.json to contain {}.",
                dep
            );
        }
    }

    #[test]
    fn test_aliases_are_applied() {
        let rust_project_path = PathBuf::from(env::var("RUST_PROJECT_JSON").unwrap());

        let content = std::fs::read_to_string(&rust_project_path)
            .unwrap_or_else(|_| panic!("couldn't open {:?}", &rust_project_path));

        let project: Project =
            serde_json::from_str(&content).expect("Failed to deserialize project JSON");

        let renamed_proc_macro_dep_index = project
            .crates
            .iter()
            .enumerate()
            .find(|(_, krate)| krate.display_name == "renamed_proc_macro_dep")
            .map(|(index, _)| index)
            .unwrap();
        let krate = project
            .crates
            .iter()
            .find(|krate| krate.display_name == "mylib")
            .unwrap();
        let dep = krate
            .deps
            .iter()
            .find(|dep| dep.krate == renamed_proc_macro_dep_index)
            .unwrap();
        assert_eq!(dep.name, "shorter_name");
    }

    #[derive(Deserialize)]
    struct Project {
        crates: Vec<Crate>,
    }

    #[derive(Debug, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
    struct Crate {
        display_name: String,
        deps: BTreeSet<Dep>,
    }

    #[derive(Debug, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
    struct Dep {
        #[serde(rename = "crate")]
        krate: usize,
        name: String,
    }
}
