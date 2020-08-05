load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_library(
    name = "utility",
    srcs = ["utility.cc"],
    hdrs = ["utility.h"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "@com_google_absl//absl/time",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "adaptive_load_controller_test",
    srcs = ["adaptive_load_controller_test.cc"],
    repository = "@envoy",
    deps = [
        ":utility",
        "//source/adaptive_load:adaptive_load_controller_impl",
        "//source/adaptive_load:scoring_function_impl",
        "//source/client:nighthawk_client_lib",
        "@com_github_grpc_grpc//:grpc++_test",
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
