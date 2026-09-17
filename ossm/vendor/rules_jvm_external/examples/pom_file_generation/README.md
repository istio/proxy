# Generating `<dependency>` nodes for a POM file

This example shows how you can generate the `<dependencies>` section of a POM
file using `rules_jvm_external`'s generated targets and the `pom_file` rule from
`bazel-common`.

To generate the XML for the example Java library, build the `//:my_library_pom`
target:

```
$ bazel build //:my_library_pom
```

The generated `my_library_pom.xml` will then contain the dependency nodes for
the Java library's direct dependencies:

```
$ cat bazel-bin/my_library_pom.xml
```

```xml
<dependency>
  <groupId>com.google.guava</groupId>
  <artifactId>guava</artifactId>
  <version>27.1-jre</version>
</dependency>
<dependency>
  <groupId>com.google.inject</groupId>
  <artifactId>guice</artifactId>
  <version>4.0</version>
</dependency>
```
