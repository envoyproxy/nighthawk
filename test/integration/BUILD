load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_package",
)
load("@nh_pip3//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_binary", "py_library")

licenses(["notice"])  # Apache 2

envoy_package()

filegroup(
    name = "test_server_configs",
    data = glob(["configurations/*"]),
)

py_library(
    name = "subprocess_mixin_lib",
    srcs = [
        "subprocess_mixin.py",
    ],
)

py_library(
    name = "utility_lib",
    srcs = [
        "utility.py",
    ],
)

# envoy binary with Logical DNS support
envoy_cc_binary(
    name = "envoy-static-testonly",
    linkopts = [
        "-latomic",
        "-lrt",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//source/exe:envoy_main_entry_lib",
        "@envoy//source/extensions/clusters/logical_dns:logical_dns_cluster_lib",
        "@envoy//source/extensions/clusters/original_dst:original_dst_cluster_lib",
        "@envoy//source/extensions/load_balancing_policies/cluster_provided:config",
    ],
)

py_library(
    name = "integration_test_base",
    data = [
        ":envoy-static-testonly",
        ":test_server_configs",
        "//:nighthawk_client_testonly",
        "//:nighthawk_output_transform",
        "//:nighthawk_service",
        "//:nighthawk_test_server",
        "//test/user_defined_output/fake_plugin:fake_user_defined_output",
        "//test/user_defined_output/fake_plugin:fake_user_defined_output_proto_py_proto",
        "@envoy//test/config/integration/certs",
    ],
    deps = [
        ":integration_test_base_lean",
    ],
)

py_library(
    name = "integration_test_base_lean",
    srcs = [
        "asserts.py",
        "common.py",
        "integration_test_fixtures.py",
        "nighthawk_grpc_service.py",
        "nighthawk_test_server.py",
    ],
    data = [
        ":test_server_configs",
        "@envoy//test/config/integration/certs",
    ],
    deps = [
        ":subprocess_mixin_lib",
        ":utility_lib",
        "@rules_python//python/runfiles",
        requirement("requests"),
        # The following are implied by 'request'.
        requirement("urllib3"),
        requirement("chardet"),
        requirement("certifi"),
        requirement("idna"),
        requirement("pytest"),
        requirement("pytest-dependency"),
        # The following are implied by pytest & co.
        requirement("attrs"),
        requirement("pluggy"),
        requirement("zipp"),
        requirement("six"),
        requirement("py"),
        requirement("more_itertools"),
        requirement("importlib_metadata"),
        requirement("packaging"),
        requirement("pytest-xdist"),
        requirement("execnet"),
        requirement("apipkg"),
        requirement("pyyaml"),
        requirement("iniconfig"),
    ],
)

py_library(
    name = "test_integration_basics_lib",
    srcs = ["test_integration_basics.py"],
    deps = [":integration_test_base"],
)

py_library(
    name = "test_connection_management_lib",
    srcs = ["test_connection_management.py"],
    deps = [":integration_test_base"],
)

py_library(
    name = "test_integration_zipkin_lib",
    srcs = ["test_integration_zipkin.py"],
    deps = [":integration_test_base"],
)

py_library(
    name = "test_grpc_service_lib",
    srcs = ["test_grpc_service.py"],
    deps = [":integration_test_base"],
)

py_library(
    name = "test_remote_execution_lib",
    srcs = ["test_remote_execution.py"],
    deps = [":integration_test_base"],
)

py_library(
    name = "test_output_transform_lib",
    srcs = ["test_output_transform.py"],
    deps = [":integration_test_base"],
)

py_library(
    name = "test_request_source_plugin_lib",
    srcs = ["test_request_source_plugin.py"],
    data = ["//test/request_source:testdata"],
    deps = [":integration_test_base"],
)

py_library(
    name = "test_user_defined_output_plugins_lib",
    srcs = ["test_user_defined_output_plugins.py"],
    deps = [":integration_test_base"],
)

py_binary(
    name = "integration_test",
    srcs = ["integration_test.py"],
    args = select({
        "//bazel:zipkin_disabled": [
            "\"not zipkin\"",
        ],
        "//conditions:default": [],
    }),
    deps = [
        ":test_connection_management_lib",
        ":test_grpc_service_lib",
        ":test_integration_basics_lib",
        ":test_integration_zipkin_lib",
        ":test_output_transform_lib",
        ":test_remote_execution_lib",
        ":test_request_source_plugin_lib",
        ":test_user_defined_output_plugins_lib",
    ],
)
