load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

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
        ":minimal_output",
        "//include/nighthawk/adaptive_load:adaptive_load_controller",
        "//source/adaptive_load:adaptive_load_client_main",
        "//test/mocks/adaptive_load:mock_adaptive_load_controller",
        "//test/test_common:environment_lib",
        "@com_github_grpc_grpc//:grpc++_test",
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
        ":minimal_output",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_load:input_variable_setter",
        "//include/nighthawk/adaptive_load:step_controller",
        "//source/adaptive_load:adaptive_load_controller_impl",
        "//source/adaptive_load:scoring_function_impl",
        "//source/adaptive_load:session_spec_proto_helper_impl",
        "//test/adaptive_load/fake_plugins/fake_step_controller",
        "//test/common:fake_time_source",
        "//test/mocks/adaptive_load:mock_metrics_evaluator",
        "//test/mocks/adaptive_load:mock_session_spec_proto_helper",
        "//test/mocks/common:mock_nighthawk_service_client",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
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
    name = "metrics_evaluator_test",
    srcs = ["metrics_evaluator_test.cc"],
    repository = "@envoy",
    deps = [
        ":minimal_output",
        "//source/adaptive_load:metrics_evaluator_impl",
        "//source/adaptive_load:scoring_function_impl",
        "//test/adaptive_load/fake_plugins/fake_metrics_plugin",
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
    name = "plugin_loader_test",
    srcs = ["plugin_loader_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:plugin_loader",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "session_spec_proto_helper_test",
    srcs = ["session_spec_proto_helper_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:session_spec_proto_helper_impl",
        "//test/adaptive_load/fake_plugins/fake_metrics_plugin",
        "//test/adaptive_load/fake_plugins/fake_step_controller",
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
        "//source/adaptive_load:step_controller_impl",
        "//test/adaptive_load/fake_plugins/fake_input_variable_setter",
        "//test/adaptive_load/fake_plugins/fake_metrics_plugin",
    ],
)
