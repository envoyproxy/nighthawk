load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

licenses(["notice"])  # Apache 2

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
    deps = [
        "@rules_python//python/runfiles",
    ],
)

py_library(
    name = "benchmarks_dynamic_envoy_proxy_lib",
    srcs = [
        "dynamic_config_envoy_proxy.py",
        "envoy_proxy.py",
    ],
    data = [
        "configurations/dynamic_resources.yaml",
        "configurations/lds.yaml",
        "test/templates/simple_plot.html",
    ],
    srcs_version = "PY2AND3",
)

py_test(
    name = "test_benchmarks_dynamic_envoy",
    srcs = [
        "benchmarks.py",
        "test/test_cds_churn_with_traffic.py",
    ],
    main = "benchmarks.py",
    srcs_version = "PY2AND3",
    deps = [
        ":benchmarks_dynamic_envoy_proxy_lib",
        "//dynamic_config:dynamic_config_server",
        "//test/integration:integration_test_base",
    ],
)

py_binary(
    name = "dynamic_benchmarks",
    srcs = [
        "benchmarks.py",
    ],
    main = "benchmarks.py",
    srcs_version = "PY2AND3",
    deps = [
        ":benchmarks_dynamic_envoy_proxy_lib",
        "//test/integration:integration_test_base_lean",
    ],
)
