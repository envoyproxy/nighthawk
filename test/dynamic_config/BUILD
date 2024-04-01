load("@nh_pip3//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_test")

licenses(["notice"])  # Apache 2

py_test(
    name = "dynamic_config_manager_test",
    srcs = [
        "dynamic_config_manager_test.py",
    ],
    srcs_version = "PY3",
    deps = [
        "//dynamic_config:dynamic_config_manager_lib",
        requirement("pytest"),
    ],
)
