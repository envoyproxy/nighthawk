licenses(["notice"])  # Apache 2

load("@rules_python//python:defs.bzl", "py_binary")
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)

envoy_package()

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
