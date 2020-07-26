load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_library(
    name = "utility_lib",
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
    name = "adaptive_load_client_main_test",
    srcs = ["adaptive_load_client_main_test.cc"],
    data = [
        "test_data/golden_output.textproto",
        "test_data/invalid_session_spec.textproto",
        "test_data/valid_session_spec.textproto",
    ],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:adaptive_load_client_main_lib",
        "//source/adaptive_load:scoring_function_impl_lib",
        "//source/adaptive_load:step_controller_impl_lib",
        "//test/adaptive_load:utility_lib",
        "//test/test_common:environment_lib",
        "@com_github_grpc_grpc//:grpc++_test",
        "@envoy//source/common/filesystem:filesystem_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/mocks/filesystem:filesystem_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "adaptive_load_controller_test",
    srcs = ["adaptive_load_controller_test.cc"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/adaptive_load:step_controller_include",
        "//source/adaptive_load:adaptive_load_controller_impl_lib",
        "//source/adaptive_load:scoring_function_impl_lib",
        "//test/adaptive_load:utility_lib",
        "@com_github_grpc_grpc//:grpc++_test",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "input_variable_setter_test",
    srcs = ["input_variable_setter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:input_variable_setter_impl_lib",
        "//source/adaptive_load:plugin_util_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "scoring_function_test",
    srcs = ["scoring_function_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:plugin_util_lib",
        "//source/adaptive_load:scoring_function_impl_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "step_controller_test",
    srcs = ["step_controller_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:plugin_util_lib",
        "//source/adaptive_load:step_controller_impl_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "metrics_plugin_test",
    srcs = ["metrics_plugin_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:metrics_plugin_impl_lib",
        "//source/adaptive_load:plugin_util_lib",
        "//test/adaptive_load:utility_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "plugin_util_test",
    srcs = ["plugin_util_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:input_variable_setter_impl_lib",
        "//source/adaptive_load:metrics_plugin_impl_lib",
        "//source/adaptive_load:plugin_util_lib",
        "//source/adaptive_load:scoring_function_impl_lib",
        "//source/adaptive_load:step_controller_impl_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)
