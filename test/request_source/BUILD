load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

filegroup(
    name = "testdata",
    srcs = [
        "test_data/test-config.yaml",
    ],
)

envoy_cc_test_library(
    name = "stub_plugin_impl",
    srcs = [
        "stub_plugin_impl.cc",
    ],
    hdrs = [
        "stub_plugin_impl.h",
    ],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/request_source:request_source_plugin_config_factory_lib",
        "//source/common:nighthawk_common_lib",
        "//source/common:request_impl_lib",
        "//source/common:request_source_impl_lib",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//source/exe:platform_header_lib_with_external_headers",
        "@envoy//source/exe:platform_impl_lib",
    ],
)

envoy_cc_test(
    name = "request_source_plugin_test",
    srcs = ["request_source_plugin_test.cc"],
    data = [
        "test_data/test-config.yaml",
    ],
    repository = "@envoy",
    deps = [
        "//source/request_source:request_options_list_plugin_impl",
        "//test/request_source:stub_plugin_impl",
        "//test/test_common:environment_lib",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//test/mocks/api:api_mocks",
    ],
)
