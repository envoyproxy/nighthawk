load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_library",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

envoy_cc_test_library(
    name = "environment_lib",
    hdrs = ["environment.h"],
    repository = "@envoy",
    deps = [
        "@envoy//test/test_common:environment_lib",
    ],
)
