// Without --remap-path-prefix, Location::caller() in this generic function will return an absolute path
pub fn get_file_name<T>() -> &'static str {
    std::panic::Location::caller().file()
}
