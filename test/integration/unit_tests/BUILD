load("@rules_python//python:defs.bzl", "py_test")

licenses(["notice"])  # Apache 2

py_test(
    name = "test_nighthawk_test_server",
    srcs = ["test_nighthawk_test_server.py"],
    deps = [
        "//test/integration:integration_test_base_lean",
    ],
)

py_test(
    name = "test_utility",
    srcs = ["test_utility.py"],
    deps = [
        "//test/integration:integration_test_base_lean",
    ],
)
