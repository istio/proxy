# Robolectric test example using android_local_test

To run the Robolectric test in this workspace:

```
$ bazel test //src/test:main_activity_test
```

Roboelectric tests in Bazel requires special setup to generate a file called
`robolectric-deps.properties`, containing information about android-all jars and
their locations. You can find this setup in the
[robolectric-bazel/bazel/BUILD](https://github.com/robolectric/robolectric-bazel/blob/master/bazel/BUILD).
