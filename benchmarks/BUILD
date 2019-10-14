licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)

envoy_package()

py_test(
    name = "benchmarks",
    srcs = [
        "benchmarks.py",
        "test_benchmark.py",
    ],
    deps = [
        "//test/integration:integration_test_base",
        "//test/integration:test_integration_basics_lib",
    ],
)
