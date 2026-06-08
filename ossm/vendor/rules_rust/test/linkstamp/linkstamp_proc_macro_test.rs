#[test]
fn test_linkstamp_proc_macro() {
    assert_eq!(42, linkstamp_proc_macro::num_from_linkstamp!());
}
