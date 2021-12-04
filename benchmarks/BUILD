load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

licenses(["notice"])

py_binary(
    name = "benchmarks",
    srcs = [
        "benchmarks.py",
    ],
    srcs_version = "PY2AND3",
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
    srcs_version = "PY2AND3",
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
    srcs_version = "PY2AND3",
)
