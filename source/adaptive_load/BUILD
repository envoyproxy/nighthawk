load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "plugin_util",
    srcs = [
        "plugin_util.cc",
    ],
    hdrs = [
        "plugin_util.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/adaptive_load:input_variable_setter",
        "//include/nighthawk/adaptive_load:metrics_plugin",
        "//include/nighthawk/adaptive_load:scoring_function",
        "//include/nighthawk/adaptive_load:step_controller",
        "//source/common:nighthawk_common_lib",
        "@envoy//source/common/config:utility_lib_with_external_headers",
    ],
)
