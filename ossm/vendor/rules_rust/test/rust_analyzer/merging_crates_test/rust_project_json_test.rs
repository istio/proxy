#[cfg(test)]
mod tests {
    use serde::Deserialize;
    use std::env;
    use std::path::PathBuf;

    #[derive(Deserialize)]
    struct Project {
        crates: Vec<Crate>,
    }

    #[derive(Deserialize)]
    struct Crate {
        display_name: String,
        deps: Vec<Dep>,
    }

    #[derive(Deserialize)]
    struct Dep {
        name: String,
    }

    #[test]
    fn test_deps_of_crate_and_its_test_are_merged() {
        let rust_project_path = PathBuf::from(env::var("RUST_PROJECT_JSON").unwrap());
        let content = std::fs::read_to_string(&rust_project_path)
            .unwrap_or_else(|_| panic!("couldn't open {:?}", &rust_project_path));
        println!("{}", content);
        let project: Project =
            serde_json::from_str(&content).expect("Failed to deserialize project JSON");

        let lib = project
            .crates
            .iter()
            .find(|c| &c.display_name == "mylib")
            .unwrap();
        let mut deps = lib.deps.iter().map(|d| &d.name).collect::<Vec<_>>();
        deps.sort();
        assert!(deps == vec!["extra_test_dep", "lib_dep"]);
    }
}
