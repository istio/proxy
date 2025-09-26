# Examples of how time stamping works.

## How it works

Target declarations may use the `stamp` attribute to control
the time stamping of files in an archive. The behavior follows
the pattern of the cc_binary rule:

https://docs.bazel.build/versions/main/be/c-cpp.html#cc_binary

Read the BUILD file for more details.

## Try this

```
bazel build :*
for tarball in bazel-bin/*.tar ; do
  echo ==== $tarball
  tar tvf $tarball
done

bazel build :*  --stamp=1
for tarball in bazel-bin/*.tar ; do
  echo ==== $tarball
  tar tvf $tarball
done
```

You should see something like:
```
INFO: Build completed successfully, 3 total actions
==== bazel-bin/always_stamped.tar
-r-xr-xr-x  0 0      0         968 May  3 17:34 BUILD
==== bazel-bin/controlled_by_stamp_option.tar
-r-xr-xr-x  0 0      0         968 Dec 31  1999 BUILD
==== bazel-bin/never_stamped.tar
-r-xr-xr-x  0 0      0         968 Dec 31  1999 BUILD
INFO: Build option --stamp has changed, discarding analysis cache.
INFO: Build completed successfully, 3 total actions
==== bazel-bin/always_stamped.tar
-r-xr-xr-x  0 0      0         968 May  3 17:34 BUILD
==== bazel-bin/controlled_by_stamp_option.ta
-r-xr-xr-x  0 0      0         968 May  6 17:42 BUILD
==== bazel-bin/never_stamped.tar
-r-xr-xr-x  0 0      0         968 Dec 31  1999 BUILD
```
