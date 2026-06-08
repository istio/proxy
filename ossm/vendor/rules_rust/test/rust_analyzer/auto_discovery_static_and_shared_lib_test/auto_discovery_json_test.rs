#[cfg(test)]
mod tests {
    use serde::Deserialize;
    use std::env;
    use std::path::PathBuf;

    #[derive(Deserialize)]
    #[serde(tag = "kind")]
    #[serde(rename_all = "snake_case")]
    enum DiscoverProject {
        Finished { project: Project },
        Progress {},
    }

    #[derive(Deserialize)]
    struct Project {
        crates: Vec<Crate>,
    }

    #[derive(Deserialize)]
    struct Crate {
        display_name: String,
        root_module: String,
    }

    #[test]
    fn test_static_and_shared_lib() {
        let rust_project_path = PathBuf::from(env::var("AUTO_DISCOVERY_JSON").unwrap());
        let content = std::fs::read_to_string(&rust_project_path)
            .unwrap_or_else(|_| panic!("couldn't open {:?}", &rust_project_path));
        println!("{}", content);

        for line in content.lines() {
            let discovery: DiscoverProject =
                serde_json::from_str(line).expect("Failed to deserialize discovery JSON");

            let project = match discovery {
                DiscoverProject::Finished { project } => project,
                DiscoverProject::Progress {} => continue,
            };

            let cdylib = project
                .crates
                .iter()
                .find(|c| &c.display_name == "greeter_cdylib")
                .unwrap();
            assert!(cdylib.root_module.ends_with("/shared_lib.rs"));

            let staticlib = project
                .crates
                .iter()
                .find(|c| &c.display_name == "greeter_staticlib")
                .unwrap();
            assert!(staticlib.root_module.ends_with("/static_lib.rs"));
        }
    }
}
