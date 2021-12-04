load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])

envoy_package()

api_cc_py_proto_library(
    name = "fake_step_controller_proto",
    srcs = [
        "fake_step_controller.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base",
        "@envoy_api//envoy/config/core/v3:pkg",
    ],
)

envoy_cc_test_library(
    name = "fake_step_controller",
    srcs = [
        "fake_step_controller.cc",
    ],
    hdrs = [
        "fake_step_controller.h",
    ],
    repository = "@envoy",
    deps = [
        ":fake_step_controller_proto_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_load:input_variable_setter",
        "//include/nighthawk/adaptive_load:step_controller",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_cc_test(
    name = "fake_step_controller_test",
    srcs = ["fake_step_controller_test.cc"],
    repository = "@envoy",
    deps = [
        ":fake_step_controller",
        "//source/adaptive_load:plugin_loader",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)
