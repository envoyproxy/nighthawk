load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_library(
    name = "environment_lib",
    hdrs = [
        "environment.h",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//test/test_common:environment_lib",
    ],
)

envoy_cc_test_library(
    name = "proto_matchers",
    hdrs = ["proto_matchers.h"],
    repository = "@envoy",
    deps = [
        "@envoy//test/test_common:utility_lib",
    ],
)
