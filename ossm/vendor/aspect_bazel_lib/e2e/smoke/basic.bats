bats_load_library "bats-support"
bats_load_library "bats-assert"

@test 'basic' {
	run echo 'have'
	assert_output 'have'
}
