licenses(["notice"])  # Apache 2

load("@rules_python//python:defs.bzl", "py_binary")

py_binary(
    name = "benchmarks",
    srcs = [
        "benchmarks.py",
        "test_benchmark.py",
    ],
    deps = [
        "//test/integration:integration_test_base_lean",
    ],
)
