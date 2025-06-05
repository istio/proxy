#[cfg(feature = "always_stamp")]
use default_with_build_flag_on_lib::build_timestamp;
#[cfg(feature = "always_stamp")]
use default_with_build_flag_on_lib::build_user;

#[cfg(feature = "never_stamp")]
use default_with_build_flag_off_lib::build_timestamp;
#[cfg(feature = "never_stamp")]
use default_with_build_flag_off_lib::build_user;

#[cfg(feature = "always_stamp_build_flag_true")]
use always_stamp_build_flag_true_lib::build_timestamp;
#[cfg(feature = "always_stamp_build_flag_true")]
use always_stamp_build_flag_true_lib::build_user;

#[cfg(feature = "always_stamp_build_flag_false")]
use always_stamp_build_flag_false_lib::build_timestamp;
#[cfg(feature = "always_stamp_build_flag_false")]
use always_stamp_build_flag_false_lib::build_user;

#[cfg(feature = "never_stamp_build_flag_true")]
use never_stamp_build_flag_true_lib::build_timestamp;
#[cfg(feature = "never_stamp_build_flag_true")]
use never_stamp_build_flag_true_lib::build_user;

#[cfg(feature = "never_stamp_build_flag_false")]
use never_stamp_build_flag_false_lib::build_timestamp;
#[cfg(feature = "never_stamp_build_flag_false")]
use never_stamp_build_flag_false_lib::build_user;

#[cfg(feature = "consult_cmdline_value_is_true")]
use consult_cmdline_value_is_true_lib::build_timestamp;
#[cfg(feature = "consult_cmdline_value_is_true")]
use consult_cmdline_value_is_true_lib::build_user;

#[cfg(feature = "consult_cmdline_value_is_false")]
use consult_cmdline_value_is_false_lib::build_timestamp;
#[cfg(feature = "consult_cmdline_value_is_false")]
use consult_cmdline_value_is_false_lib::build_user;

fn main() {
    println!("bin stamp: {}", env!("BUILD_TIMESTAMP"));
    println!("lib stamp: {}", build_timestamp());

    println!("bin stamp: {}", env!("BUILD_USER"));
    println!("lib stamp: {}", build_user());
}
