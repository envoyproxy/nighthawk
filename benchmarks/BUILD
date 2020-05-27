licenses(["notice"])  # Apache 2

load("@rules_python//python:defs.bzl", "py_binary", "py_test")

py_binary(
    name = "benchmarks",
    srcs = [
        "benchmarks.py",
        "infra.py",
        "test_benchmark.py",
    ],
    deps = [
        "//test/integration:integration_test_base_lean",
    ],
)

py_test(
    name = "test_benchmarks",
    srcs = [
        "benchmarks.py",
        "infra.py",
        "test_benchmark.py",
    ],
    data = ["configurations/envoy_proxy.yaml"],
    main = "benchmarks.py",
    deps = [
        "//test/integration:integration_test_base",
    ],
)
