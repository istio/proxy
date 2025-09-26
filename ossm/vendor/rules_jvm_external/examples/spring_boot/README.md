# Spring Boot Example

This is an example of the tutorial app from [Spring's
website](https://spring.io/guides/gs/spring-boot/).

To build the Spring Boot application:

```
$ bazel build //src/main/java/hello:app
```

To run the Spring Boot application from Bazel:

```
$ bazel run //src/main/java/hello:app
```

To run the tests from Bazel:

```
$ bazel test //src/test/...
```

This tutorial code is [licensed under Apache 2.0, copyright GoPivotal
Inc.](https://github.com/spring-guides/gs-spring-boot/blob/d0f3a942f1b31ee73d2896c1e201f11cd8efd6ba/LICENSE.code.txt)

### Building a Deployable Jar

The example above shows how to launch a Spring Boot application from Bazel.
Bazel builds a classpath using all of the dependency jars and launches the application.

Spring Boot also supports a deployable jar format in which the Spring Boot application is packaged as a single Java .jar file.
This use case is typical in production, where the Bazel executable and Bazel workspace are not available.

Use this external Spring Boot rule implementation if a deployable jar is needed:
- [Spring Boot rule for Bazel](https://github.com/salesforce/bazel-springboot-rule)
