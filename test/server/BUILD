load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "http_test_server_filter_integration_test",
    srcs = ["http_test_server_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/server:http_test_server_filter_config",
        "@envoy//include/envoy/upstream:cluster_manager_interface_with_external_headers",
        "@envoy//source/common/api:api_lib_with_external_headers",
        "@envoy//test/integration:http_integration_lib",
    ],
)
