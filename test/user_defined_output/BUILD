load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")
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
        "//source/user_defined_output:user_defined_output_plugin_creator",
        "//test/user_defined_output/fake_plugin:fake_user_defined_output",
        "//test/user_defined_output/fake_plugin:fake_user_defined_output_proto_cc_proto",
    ],
)
