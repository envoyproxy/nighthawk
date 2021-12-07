load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "well_known_headers_lib",
    hdrs = ["well_known_headers.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/http:headers_lib_with_external_headers",
        "@envoy//source/common/singleton:const_singleton_with_external_headers",
    ],
)

envoy_cc_library(
    name = "configuration_lib",
    srcs = [
        "configuration.cc",
        "http_filter_config_base.cc",
    ],
    hdrs = [
        "configuration.h",
        "http_filter_config_base.h",
    ],
    repository = "@envoy",
    deps = [
        ":well_known_headers_lib",
        "//api/server:response_options_proto_cc_proto",
        "@envoy//envoy/server:filter_config_interface_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//source/common/singleton:const_singleton_with_external_headers",
        "@envoy_api//envoy/api/v2/core:pkg_cc_proto",
        "@envoy_api//envoy/config/core/v3:pkg_cc_proto",
    ],
)

envoy_cc_library(
    name = "http_test_server_filter_lib",
    srcs = ["http_test_server_filter.cc"],
    hdrs = ["http_test_server_filter.h"],
    repository = "@envoy",
    deps = [
        ":configuration_lib",
        "//api/server:response_options_proto_cc_proto",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_time_tracking_filter_lib",
    srcs = ["http_time_tracking_filter.cc"],
    hdrs = ["http_time_tracking_filter.h"],
    repository = "@envoy",
    deps = [
        ":configuration_lib",
        "//api/server:response_options_proto_cc_proto",
        "//source/common:thread_safe_monotonic_time_stopwatch_lib",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_dynamic_delay_filter_lib",
    srcs = ["http_dynamic_delay_filter.cc"],
    hdrs = ["http_dynamic_delay_filter.h"],
    repository = "@envoy",
    deps = [
        ":configuration_lib",
        "//api/server:response_options_proto_cc_proto",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
        "@envoy//source/extensions/filters/http/fault:fault_filter_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_test_server_filter_config",
    srcs = ["http_test_server_filter_config.cc"],
    repository = "@envoy",
    deps = [
        ":http_test_server_filter_lib",
        "@envoy//envoy/server:filter_config_interface_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_dynamic_delay_filter_config",
    srcs = ["http_dynamic_delay_filter_config.cc"],
    repository = "@envoy",
    deps = [
        ":http_dynamic_delay_filter_lib",
        "@envoy//envoy/server:filter_config_interface_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_time_tracking_filter_config",
    srcs = ["http_time_tracking_filter_config.cc"],
    repository = "@envoy",
    deps = [
        ":http_time_tracking_filter_lib",
        "@envoy//envoy/server:filter_config_interface_with_external_headers",
    ],
)
