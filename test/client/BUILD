load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_library",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

envoy_cc_test_library(
    name = "utility_lib",
    srcs = ["utility.cc"],
    hdrs = [
        "utility.h",
    ],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
