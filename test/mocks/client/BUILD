licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_package",
)

envoy_package()

envoy_cc_mock(
    name = "mock_options",
    srcs = ["mock_options.cc"],
    hdrs = ["mock_options.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/client:client_includes",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
    ],
)

envoy_cc_mock(
    name = "mock_benchmark_client",
    srcs = ["mock_benchmark_client.cc"],
    hdrs = ["mock_benchmark_client.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/client:client_includes",
    ],
)

envoy_cc_mock(
    name = "mock_benchmark_client_factory",
    srcs = ["mock_benchmark_client_factory.cc"],
    hdrs = ["mock_benchmark_client_factory.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/client:client_includes",
    ],
)
