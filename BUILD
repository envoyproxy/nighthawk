load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

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
    repository = "@envoy",
    deps = [
        "//source/exe:adaptive_load_client_entry_lib",
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
        "//source/server:http_dynamic_delay_filter_config",
        "//source/server:http_test_server_filter_config",
        "//source/server:http_time_tracking_filter_config",
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
