licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_library",
    "envoy_package",
)

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
