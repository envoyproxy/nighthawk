load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "common_lib",
    srcs = ["common.cc"],
    hdrs = ["common.h"],
    repository = "@envoy",
    deps = [
        "//api/server:response_options_proto_cc_proto",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//source/common/singleton:const_singleton_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_test_server_filter_lib",
    srcs = ["http_test_server_filter.cc"],
    hdrs = ["http_test_server_filter.h"],
    repository = "@envoy",
    deps = [
        ":common_lib",
        "//api/server:response_options_proto_cc_proto",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_dynamic_delay_filter_lib",
    srcs = ["http_dynamic_delay_filter.cc"],
    hdrs = ["http_dynamic_delay_filter.h"],
    repository = "@envoy",
    deps = [
        ":common_lib",
        "//api/server:response_options_proto_cc_proto",
        "@envoy//source/common/stats:symbol_table_lib_with_external_headers",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_test_server_filter_config",
    srcs = ["http_test_server_filter_config.cc"],
    repository = "@envoy",
    deps = [
        ":http_test_server_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_dynamic_delay_filter_config",
    srcs = ["http_dynamic_delay_filter_config.cc"],
    repository = "@envoy",
    deps = [
        ":http_dynamic_delay_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface_with_external_headers",
    ],
)
