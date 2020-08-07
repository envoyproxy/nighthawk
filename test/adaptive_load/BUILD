load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "input_variable_setter_test",
    srcs = ["input_variable_setter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:input_variable_setter_impl",
    ],
)

envoy_cc_test(
    name = "plugin_util_test",
    srcs = ["plugin_util_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:plugin_util",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)
