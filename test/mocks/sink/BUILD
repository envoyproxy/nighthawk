load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_package",
)

licenses(["notice"])

envoy_package()

envoy_cc_mock(
    name = "mock_sink",
    srcs = ["mock_sink.cc"],
    hdrs = ["mock_sink.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/sink:sink_lib",
    ],
)
