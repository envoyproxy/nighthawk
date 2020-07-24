load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "adaptive_load_controller_include",
    hdrs = [
        "adaptive_load_controller.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
    ],
)

envoy_basic_cc_library(
    name = "input_variable_setter_include",
    hdrs = [
        "input_variable_setter.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
    ],
)

envoy_basic_cc_library(
    name = "metrics_plugin_include",
    hdrs = [
        "metrics_plugin.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
    ],
)

envoy_basic_cc_library(
    name = "scoring_function_include",
    hdrs = [
        "scoring_function.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
    ],
)

envoy_basic_cc_library(
    name = "step_controller_include",
    hdrs = [
        "step_controller.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
    ],
)
