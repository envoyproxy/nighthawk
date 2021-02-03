load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_mock(
    name = "mock_adaptive_load_controller",
    srcs = ["mock_adaptive_load_controller.cc"],
    hdrs = ["mock_adaptive_load_controller.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/adaptive_load:adaptive_load_controller",
    ],
)

envoy_cc_mock(
    name = "mock_metrics_evaluator",
    srcs = ["mock_metrics_evaluator.cc"],
    hdrs = ["mock_metrics_evaluator.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/adaptive_load:metrics_evaluator",
    ],
)

envoy_cc_mock(
    name = "mock_session_spec_proto_helper",
    srcs = ["mock_session_spec_proto_helper.cc"],
    hdrs = ["mock_session_spec_proto_helper.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/adaptive_load:session_spec_proto_helper",
    ],
)
