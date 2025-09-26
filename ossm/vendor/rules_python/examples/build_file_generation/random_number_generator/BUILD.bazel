load("@rules_python//python:py_library.bzl", "py_library")
load("@rules_python//python:py_test.bzl", "py_test")

py_library(
    name = "random_number_generator",
    srcs = [
        "__init__.py",
        "generate_random_number.py",
    ],
    visibility = ["//:__subpackages__"],
)

py_test(
    name = "random_number_generator_test",
    srcs = ["__test__.py"],
    main = "__test__.py",
    deps = [":random_number_generator"],
)
