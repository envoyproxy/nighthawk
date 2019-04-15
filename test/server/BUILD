load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
)

package(default_visibility = ["//visibility:public"])

envoy_cc_test(
    name = "http_test_server_filter_integration_test",
    srcs = ["http_test_server_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/server:http_test_server_filter_config",
        "@envoy//include/envoy/upstream:cluster_manager_interface",
        "@envoy//source/common/api:api_lib",
        "@envoy//test/integration:http_integration_lib",
    ],
)
