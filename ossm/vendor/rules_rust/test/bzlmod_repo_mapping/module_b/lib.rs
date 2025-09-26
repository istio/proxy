use runfiles::Runfiles;

pub fn read_file_from_module_c() -> String {
    let r = Runfiles::create().unwrap();
    std::fs::read_to_string(runfiles::rlocation!(r, "aliased_c/MODULE.bazel").unwrap()).unwrap()
}
