load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

licenses(["notice"])  # Apache 2

py_binary(
    name = "benchmarks",
    srcs = [
        "benchmarks.py",
    ],
    deps = [
        ":benchmarks_envoy_proxy_lib",
        "//test/integration:integration_test_base_lean",
    ],
)

py_test(
    name = "test_benchmarks",
    srcs = [
        "benchmarks.py",
        "test/test_discovery.py",
    ],
    main = "benchmarks.py",
    deps = [
        ":benchmarks_envoy_proxy_lib",
        "//test/integration:integration_test_base",
    ],
)

py_library(
    name = "benchmarks_envoy_proxy_lib",
    srcs = [
        "envoy_proxy.py",
    ],
    data = [
        "configurations/envoy_proxy.yaml",
        "test/templates/simple_plot.html",
    ],
)
