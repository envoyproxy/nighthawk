licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "http_test_server_filter_lib",
    srcs = ["http_test_server_filter.cc"],
    hdrs = ["http_test_server_filter.h"],
    repository = "@envoy",
    deps = [
        "//api/server:response_options_proto_cc",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "http_test_server_filter_config",
    srcs = ["http_test_server_filter_config.cc"],
    repository = "@envoy",
    deps = [
        ":http_test_server_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface",
    ],
)
