load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_library(
    name = "fake_time_source",
    srcs = ["fake_time_source.cc"],
    hdrs = ["fake_time_source.h"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "@com_google_absl//absl/time",
    ],
)

envoy_cc_test_library(
    name = "minimal_output",
    srcs = ["minimal_output.cc"],
    hdrs = ["minimal_output.h"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
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
        "//source/adaptive_load:adaptive_load_client_main",
        "//source/adaptive_load:scoring_function_impl",
        "//source/adaptive_load:step_controller_impl",
        ":fake_time_source",
        ":minimal_output",
        "//test/test_common:environment_lib",
        "@com_github_grpc_grpc//:grpc++_test",
        # "@envoy//source/common/filesystem:filesystem_lib",
        "@envoy//source/common/filesystem:filesystem_lib_with_external_headers",
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
        ":fake_time_source",
        ":minimal_output",
        "//test/adaptive_load/fake_plugins/fake_step_controller",
        "//source/adaptive_load:adaptive_load_controller_impl",
        "//source/adaptive_load:scoring_function_impl",
        "//source/client:nighthawk_client_lib",
        "@com_github_grpc_grpc//:grpc++_test",
    ],
)

envoy_cc_test(
    name = "input_variable_setter_test",
    srcs = ["input_variable_setter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:input_variable_setter_impl",
    ],
)

envoy_cc_test(
    name = "metrics_plugin_test",
    srcs = ["metrics_plugin_test.cc"],
    repository = "@envoy",
    deps = [
        ":minimal_output",
        "//source/adaptive_load:metrics_plugin_impl",
    ],
)

envoy_cc_test(
    name = "scoring_function_test",
    srcs = ["scoring_function_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:scoring_function_impl",
    ],
)

envoy_cc_test(
    name = "step_controller_test",
    srcs = ["step_controller_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:config_validator_impl",
        "//source/adaptive_load:step_controller_impl",
    ],
)

envoy_cc_test(
    name = "plugin_loader_test",
    srcs = ["plugin_loader_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:plugin_loader",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)
