load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_library(
    name = "http_filter_integration_test_base_lib",
    srcs = ["http_filter_integration_test_base.cc"],
    hdrs = ["http_filter_integration_test_base.h"],
    repository = "@envoy",
    deps = [
        "//source/server:well_known_headers_lib",
        "@envoy//test/integration:http_integration_lib",
    ],
)

envoy_cc_test(
    name = "http_test_server_filter_integration_test",
    srcs = ["http_test_server_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        ":http_filter_integration_test_base_lib",
        "//source/server:http_test_server_filter_config",
        "@envoy//source/common/api:api_lib_with_external_headers",
    ],
)

envoy_cc_test(
    name = "http_dynamic_delay_filter_integration_test",
    srcs = ["http_dynamic_delay_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        ":http_filter_integration_test_base_lib",
        "//source/server:http_dynamic_delay_filter_config",
        "@envoy//source/common/api:api_lib_with_external_headers",
    ],
)

envoy_cc_test(
    name = "http_time_tracking_filter_integration_test",
    srcs = ["http_time_tracking_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        ":http_filter_integration_test_base_lib",
        "//source/server:http_time_tracking_filter_config",
        "@envoy//source/common/api:api_lib_with_external_headers",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)
