pub(crate) const fn get_opt_level() -> &'static str {
    env!("BUILD_SCRIPT_OPT_LEVEL")
}

pub(crate) const fn get_out_dir() -> &'static str {
    env!("BUILD_SCRIPT_OUT_DIR")
}
