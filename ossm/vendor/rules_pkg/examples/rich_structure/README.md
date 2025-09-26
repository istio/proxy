# Example of how pkg_* rules can compose to create a rich package structure.

## Use case

Our use case is building a distribution package that represents a typical \*nix tool.
That would include elements such as:

- A main program that requires runtime support files
- Associated documentation
- A service associated with the main binary
- Associate configuration files

where each elements must be installed in a specific place in the file system.
The final package might look like this for Linux:

```
etc/foo.rc
etc/food.conf
sbin/food
usr/bin/foo          # symlink to ../share/foo/bin/foo
usr/bin/fooctl       # symlink to ../share/foo/bin/fooctl
usr/bin/foocheck
usr/lib/foo/runtime.so
usr/lib/foo/runtime.so
usr/share/doc/foo/copyright
usr/share/doc/foo/README.txt
usr/share/doc/foo/foo.html
usr/share/man/man1/foo.1.gz
usr/share/man/man1/fooctl.1.gz
usr/share/man/man8/food.8.gz
usr/share/foo/bin/foo
usr/share/foo/bin/fooctl
usr/share/foo/bar.rules
usr/share/foo/baz.rules
usr/share/foo/locale/foo/en/msgs.cat
usr/share/foo/locale/foo/it/msgs.cat
usr/share/foo/locale/fooctl/en/msgs.cat
usr/share/foo/locale/fooctl/it/msgs.cat
usr/share/foo/locale/food/en/msgs.cat
usr/share/foo/locale/food/it/msgs.cat
var/tmp/foo
var/tmp/foo/queue
```

For macOS, it would be mostly the same, but files under `usr/share/foo` would
move to `Library/Foo`.

To emulate reality better, the source tree is organized in a way that is
convenient for the developers. This example illustrates techniques to create
the desired final structure.
