load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "request_source_plugin_test",
    srcs = ["request_source_plugin_test.cc"],
    data = [
        "test_data/test-config.yaml",
    ],
    repository = "@envoy",
    deps = [
        "//source/common:request_source_plugin_impl",
        "//test/test_common:environment_lib",
        "@envoy//source/common/config:utility_lib_with_external_headers",
    ],
)
