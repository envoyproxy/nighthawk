load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_mock(
    name = "mock_nighthawk_service_client",
    srcs = ["mock_nighthawk_service_client.cc"],
    hdrs = ["mock_nighthawk_service_client.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:nighthawk_service_client",
    ],
)

envoy_cc_mock(
    name = "mock_rate_limiter",
    srcs = ["mock_rate_limiter.cc"],
    hdrs = ["mock_rate_limiter.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
    ],
)

envoy_cc_mock(
    name = "mock_sequencer",
    srcs = ["mock_sequencer.cc"],
    hdrs = ["mock_sequencer.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
    ],
)

envoy_cc_mock(
    name = "mock_sequencer_factory",
    srcs = ["mock_sequencer_factory.cc"],
    hdrs = ["mock_sequencer_factory.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
    ],
)

envoy_cc_mock(
    name = "mock_termination_predicate",
    srcs = ["mock_termination_predicate.cc"],
    hdrs = ["mock_termination_predicate.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
    ],
)

envoy_cc_mock(
    name = "mock_termination_predicate_factory",
    srcs = ["mock_termination_predicate_factory.cc"],
    hdrs = ["mock_termination_predicate_factory.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
    ],
)

envoy_cc_mock(
    name = "mock_platform_util",
    srcs = ["mock_platform_util.cc"],
    hdrs = ["mock_platform_util.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
    ],
)

envoy_cc_mock(
    name = "mock_request_source",
    srcs = ["mock_request_source.cc"],
    hdrs = ["mock_request_source.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
    ],
)

envoy_cc_mock(
    name = "mock_request_source_factory",
    srcs = ["mock_request_source_factory.cc"],
    hdrs = ["mock_request_source_factory.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
    ],
)

envoy_cc_mock(
    name = "mock_sink",
    srcs = ["mock_sink.cc"],
    hdrs = ["mock_sink.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:sink_lib",
    ],
)
