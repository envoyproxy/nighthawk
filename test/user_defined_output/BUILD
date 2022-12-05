load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "user_defined_output_plugin_creator_test",
    srcs = ["user_defined_output_plugin_creator_test.cc"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/user_defined_output:user_defined_output_plugin",
        "//source/user_defined_output:user_defined_output_plugin_creator",
        "//test/user_defined_output/fake_plugin:fake_user_defined_output",
        "//test/user_defined_output/fake_plugin:fake_user_defined_output_proto_cc_proto",
    ],
)

envoy_cc_test(
    name = "log_response_headers_plugin_test",
    srcs = ["log_response_headers_plugin_test.cc"],
    repository = "@envoy",
    deps = [
        "//api/user_defined_output:log_response_headers_proto_cc_proto",
        "//include/nighthawk/user_defined_output:user_defined_output_plugin",
        "//source/user_defined_output:log_response_headers_plugin",
        "//test/test_common:proto_matchers",
        "@envoy//test/mocks/buffer:buffer_mocks",
    ],
)
