licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_package",
)

envoy_package()

filegroup(
    name = "nighthawk",
    srcs = [
        ":nighthawk_client",
        ":nighthawk_output_transform",
        ":nighthawk_service",
        ":nighthawk_test_server",
    ],
)

envoy_cc_binary(
    name = "nighthawk_client",
    repository = "@envoy",
    deps = [
        "//source/exe:nighthawk_client_entry_lib",
    ],
)

envoy_cc_binary(
    name = "nighthawk_test_server",
    repository = "@envoy",
    deps = [
        "//source/server:http_test_server_filter_config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

envoy_cc_binary(
    name = "nighthawk_service",
    repository = "@envoy",
    deps = [
        "//source/exe:nighthawk_service_entry_lib",
    ],
)

envoy_cc_binary(
    name = "nighthawk_output_transform",
    repository = "@envoy",
    deps = [
        "//source/exe:output_transform_main_entry_lib",
    ],
)
