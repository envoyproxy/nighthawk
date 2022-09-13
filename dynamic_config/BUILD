load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")
load("@python_pip_deps//:requirements.bzl", "requirement")

licenses(["notice"])  # Apache 2

py_test(
    name = "dynamic_config_manager_test",
    srcs = [
        "dynamic_config_manager_test.py",
    ],
    srcs_version = "PY3",
    deps = [
        ":dynamic_config_manager_lib",
        requirement("pytest"),
    ],
)

py_library(
    name = "dynamic_config_manager_lib",
    srcs = [
        "dynamic_config_manager.py",
    ],
    srcs_version = "PY3",
    deps = [
        "//api/configuration:cluster_config_manager_py_proto",
        "//api/configuration:endpoints_config_manager_py_proto",
        "@envoy_api//envoy/config/cluster/v3:pkg_py_proto",
        "@envoy_api//envoy/config/endpoint/v3:pkg_py_proto",
        "@envoy_api//envoy/extensions/filters/network/http_connection_manager/v3:pkg_py_proto",
        "@envoy_api//envoy/extensions/transport_sockets/tls/v3:pkg_py_proto",
        "@envoy_api//envoy/extensions/upstreams/http/v3:pkg_py_proto",
        "@envoy_api//envoy/service/discovery/v3:pkg_py_proto",
        requirement("pyyaml"),
    ],
)

py_binary(
    name = "dynamic_config_manager",
    srcs = ["dynamic_config_manager.py"],
    python_version = "PY3",
    deps = [":dynamic_config_manager_lib"],
)

py_binary(
    name = "dynamic_config_server",
    srcs = ["dynamic_config_server.py"],
    data = [
        ":dynamic_config_manager",
    ],
    python_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        "//test/integration:subprocess_mixin_lib",
        "@rules_python//python/runfiles",
    ],
)
