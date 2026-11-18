bats_load_library "bats-support"
bats_load_library "bats-assert"

setup_file() {
  cd "$BATS_TEST_DIRNAME/$WKSP"

  export PATH="$PATH:$BATS_TEST_DIRNAME/credential-helper/"
  export TAIL_PID="$(mktemp)"
  export AUTH_STDIN="$(mktemp -d)/stdin"

  mkfifo $AUTH_STDIN

  (tail -f $AUTH_STDIN & echo $! > $TAIL_PID) | CERT=$BATS_TEST_DIRNAME/ca/cert.pem KEY=$BATS_TEST_DIRNAME/ca/key.pem "$BATS_TEST_DIRNAME/$REGISTRY" 2>&3 &
  export REGISTRY_PID=$!
  export TAIL_PID=$(cat $TAIL_PID)
  
  while ! nc -z localhost 1448; do
    sleep 0.1
    echo "# waiting for registry at 1477" >&3
  done
  echo "# registry is ready. pushing the image" >&3
  "$BATS_TEST_DIRNAME/${CRANE_BIN/external/..}" push "$BATS_TEST_DIRNAME/$OCI_DIR" localhost:1448/empty_image
  echo "# image pushed" >&3
}

teardown_file() {
  echo "# cleaning up" >&3
  bazel shutdown
  kill $REGISTRY_PID
  kill $TAIL_PID
}

setup() {
  export DOCKER_CONFIG=$(mktemp -d)
  # Digest-pinned repos intentionally do not watch credentials. Force a fresh
  # fetch for each case so cached image contents cannot bypass authentication.
  bazel clean --expunge
}

function update_assert() {
  echo $@ > $AUTH_STDIN
  sleep 0.5
}

@test "plain text" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "auths": {
    "localhost:1447": { "username": "test", "password": "test" }
  }
}
EOF
    update_assert '{"Authorization": ["Basic dGVzdDp0ZXN0"]}'
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_success
}

@test "plain text base64" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "auths": {
    "http://localhost:1447": { "auth": "dGVzdDp0ZXN0" }
  }
}
EOF
    update_assert '{"Authorization": ["Basic dGVzdDp0ZXN0"]}'
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_success
}

@test "plain text https" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "auths": {
    "https://localhost:1447": { "username": "test", "password": "test" }
  }
}
EOF
    update_assert '{"Authorization": ["Basic dGVzdDp0ZXN0"]}'
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_success
}

@test "credstore" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "auths": { "localhost:1447": {} },
  "credsStore": "oci"
}
EOF
    update_assert '{"Authorization": ["Basic dGVzdGluZzpvY2k="]}'
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_success
}

@test "global credstore" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "credsStore": "oci"
}
EOF
    update_assert '{"Authorization": ["Basic dGVzdGluZzpvY2k="]}'
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_success
}

@test "credstore misbehaves" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "auths": { "localhost:1447": {} },
  "credsStore": "evil"
}
EOF
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_failure
    assert_output -p "can't run at this time" "ERROR: credential helper failed:"
}

@test "credstore missing" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "auths": { "localhost:1447": {} },
  "credsStore": "missing"
}
EOF
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_failure
    assert_output -p "exec: docker-credential-missing: not found" "ERROR: credential helper failed:"
}

@test "per registry credHelper fails" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "credHelpers": {
    "localhost:1447": "evil"
  }
}
EOF
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_failure 
    assert_output -p "Error in fail: credential helper failed:" "can't run at this time"
}

@test "per registry credHelper succeeds" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "credHelpers": {
    "localhost:1447": "new"
  }
}
EOF
    update_assert '{"Authorization": ["Basic cGVyLWNyZWQ6dGVzdGluZw=="]}'
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_success
}

@test "per registry credHelper fails to match authorization" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "credHelpers": {
    "localhost:1447": "oci"
  }
}
EOF
    update_assert '{"Authorization": ["Basic not_match"]}'
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_failure
}

@test "empty username and password succeeds" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "credHelpers": {
    "localhost:1447": "devpod-esque"
  }
}
EOF
    update_assert '{}'
    sleep 2
    run bazel build @empty_image//... $BAZEL_FLAGS
    assert_success
}


@test "custom www auth challenge works" {
    cat > "$DOCKER_CONFIG/config.json" <<EOF
{
  "auths": {
    "localhost:1447": { "auth": "dGVzdDp0ZXN0" }
  }
}
EOF
    update_assert '{"Authorization": ["Bearer here_is_the_token"]}'
    run bazel build @empty_image_with_custom_auth//... $BAZEL_FLAGS
    assert_success
}
