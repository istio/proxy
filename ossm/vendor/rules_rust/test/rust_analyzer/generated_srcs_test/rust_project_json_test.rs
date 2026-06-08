#[cfg(test)]
mod tests {
    use serde::Deserialize;
    use std::env;
    use std::path::PathBuf;

    #[derive(Deserialize)]
    struct Project {
        sysroot_src: String,
        crates: Vec<Crate>,
    }

    #[derive(Deserialize)]
    struct Crate {
        display_name: String,
        root_module: String,
        source: Option<Source>,
    }

    #[derive(Deserialize)]
    struct Source {
        include_dirs: Vec<String>,
    }

    #[test]
    fn test_generated_srcs() {
        let rust_project_path = PathBuf::from(env::var("RUST_PROJECT_JSON").unwrap());
        let content = std::fs::read_to_string(&rust_project_path)
            .unwrap_or_else(|_| panic!("couldn't open {:?}", &rust_project_path));
        let project: Project =
            serde_json::from_str(&content).expect("Failed to deserialize project JSON");

        // /tmp/_bazel/12345678/external/tools/rustlib/library => /tmp/_bazel
        let output_base = project
            .sysroot_src
            .rsplitn(2, "/external/")
            .last()
            .unwrap()
            .rsplitn(2, '/')
            .last()
            .unwrap();
        println!("output_base: {output_base}");

        let with_gen = project
            .crates
            .iter()
            .find(|c| &c.display_name == "generated_srcs")
            .unwrap();
        assert!(with_gen.root_module.starts_with("/"));
        assert!(with_gen.root_module.ends_with("/lib.rs"));

        let include_dirs = &with_gen.source.as_ref().unwrap().include_dirs;
        assert!(include_dirs.len() == 1);
        assert!(include_dirs[0].starts_with(output_base));
    }
}
