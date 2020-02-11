licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_mock(
    name = "mock_options",
    srcs = ["mock_options.cc"],
    hdrs = ["mock_options.h"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
    ],
)
