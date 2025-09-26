[![CI Status](https://oss.gprow.dev/badge.svg?jobs=jwt-verify-lib-periodic)](https://testgrid.k8s.io/googleoss-jwt-verify-lib#Summary)

This repository stores JWT verification files for c++.
These files are originally created in [istio/proxy jwt_auth folder](https://github.com/istio/proxy/blob/master/src/envoy/http/jwt_auth/jwt.h).
The key reason to create a separate repo for them is that they can be used by other projects. For example, [envoyproxy](https://github.com/envoyproxy/envoy) likes to use these code to build a jwt_auth HTTP filter.

This is not an officially supported Google product

For contributors:
If you make any changes, please make sure to use Bazel to pass all unit tests by running:

```
bazel test //...
```
Please format your codes by running:

```
script/check-style
```

## Continuous Integration 
This repository is integreated with [OSS Prow](https://github.com/GoogleCloudPlatform/oss-test-infra), and the job setup is in the [OSS Prow repo](https://github.com/GoogleCloudPlatform/oss-test-infra/blob/master/prow/prowjobs/google/jwt_verify_lib/jwt-verify-lib-presubmit.yaml). Currently, Prow runs the [presubmit script](./script/ci.sh) on each Pull Request to verify tests pass. Note:
- PR submission is only allowed if the job passes.
- If you are an outside contributor, Prow may not run until a Googler LGTMs.

The CI is running in a [docker image](./docker/Dockerfile-prow-env) that was pre-built and pushed to Google Cloud Registry. For future updates to the CI image, please refer to the commands listed in [./docker folder README](./docker/README).
