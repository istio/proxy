// Sample source that fails to compile if `--cfg=bazel_exec` is passed to rustc.
#[cfg(not(bazel_exec))]
fn exec() {}

pub fn f() {
    exec();
}
