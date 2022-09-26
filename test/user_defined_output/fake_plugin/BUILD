load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

api_cc_py_proto_library(
    name = "fake_user_defined_output_proto",
    srcs = [
        "fake_user_defined_output.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base",
        "@envoy_api//envoy/config/core/v3:pkg",
    ],
)

envoy_cc_test_library(
    name = "fake_user_defined_output",
    srcs = [
        "fake_user_defined_output.cc",
    ],
    hdrs = [
        "fake_user_defined_output.h",
    ],
    repository = "@envoy",
    deps = [
        ":fake_user_defined_output_proto_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/user_defined_output:user_defined_output_plugin",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_cc_test(
    name = "fake_user_defined_output_test",
    srcs = ["fake_user_defined_output_test.cc"],
    repository = "@envoy",
    deps = [
        ":fake_user_defined_output",
        ":fake_user_defined_output_proto_cc_proto",
        "//test/client:utility_lib",
        "//test/test_common:proto_matchers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/mocks/buffer:buffer_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)
