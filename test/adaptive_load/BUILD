load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_library(
    name = "utility_lib",
    srcs = ["utility.cc"],
    hdrs = ["utility.h"],
    repository = "@envoy",
    deps = [
        "//source/adaptive_load:adaptive_load_lib",
        "//source/client:nighthawk_client_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)
