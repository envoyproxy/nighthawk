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
