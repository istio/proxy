// Sample source that fails to compile unless `--cfg=bazel_exec` is passed to rustc.
#[cfg(bazel_exec)]
fn exec() {}

pub fn f() {
    exec();
}
