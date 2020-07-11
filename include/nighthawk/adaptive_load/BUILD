load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "adaptive_load_includes",
    hdrs = [
        "adaptive_load_controller.h",
        "input_variable_setter.h",
        "metrics_plugin.h",
        "scoring_function.h",
        "step_controller.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:base_cc_proto",
    ],
)
