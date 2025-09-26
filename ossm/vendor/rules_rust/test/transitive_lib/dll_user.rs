#[link(name = "alias")]
extern "C" {
    // random symbol from shell32
    pub fn LocalFree(ptr: *mut std::os::raw::c_void);
}
