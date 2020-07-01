load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "adaptive_rps_includes",
    hdrs = [
        "custom_metric_evaluator.h",
        "metrics_plugin.h",
        "step_controller.h",
    ],
    include_prefix = "nighthawk/adaptive_rps",
    deps = [
        "//api/adaptive_rps:base_cc_proto",
    ],
)
