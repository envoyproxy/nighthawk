load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

exports_files(["LICENSE"])

envoy_package()

filegroup(
    name = "nighthawk",
    srcs = [
        ":nighthawk_adaptive_load_client",
        ":nighthawk_client",
        ":nighthawk_output_transform",
        ":nighthawk_service",
        ":nighthawk_test_server",
    ],
)

envoy_cc_binary(
    name = "nighthawk_adaptive_load_client",
    linkopts = [
        "-l:libatomic.a",
        "-lrt",
    ],
    repository = "@envoy",
    deps = [
        "//source/exe:adaptive_load_client_entry_lib",
    ],
)

envoy_cc_binary(
    name = "nighthawk_client",
    linkopts = [
        "-l:libatomic.a",
        "-lrt",
    ],
    repository = "@envoy",
    deps = [
        "//source/exe:nighthawk_client_entry_lib",
    ],
)

# A testonly version of the nighthawk client, intended to be built with any required test plugins
# to enable integration test use cases.
envoy_cc_binary(
    name = "nighthawk_client_testonly",
    linkopts = [
        "-l:libatomic.a",
        "-lrt",
    ],
    repository = "@envoy",
    deps = [
        "//source/exe:nighthawk_client_entry_lib",
        "//source/user_defined_output:log_response_headers_plugin",
        "//test/user_defined_output/fake_plugin:fake_user_defined_output",
    ],
)

envoy_cc_binary(
    name = "nighthawk_test_server",
    linkopts = [
        "-l:libatomic.a",
        "-lrt",
    ],
    repository = "@envoy",
    deps = [
        "//source/server:http_dynamic_delay_filter_config",
        "//source/server:http_test_server_filter_config",
        "//source/server:http_time_tracking_filter_config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

envoy_cc_binary(
    name = "nighthawk_service",
    linkopts = [
        "-l:libatomic.a",
        "-lrt",
    ],
    repository = "@envoy",
    deps = [
        "//source/exe:nighthawk_service_entry_lib",
    ],
)

envoy_cc_binary(
    name = "nighthawk_output_transform",
    linkopts = [
        "-l:libatomic.a",
        "-lrt",
    ],
    repository = "@envoy",
    deps = [
        "//source/exe:output_transform_main_entry_lib",
    ],
)
