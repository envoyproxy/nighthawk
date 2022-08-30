load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_library(
    name = "fake_time_source",
    srcs = ["fake_time_source.cc"],
    hdrs = ["fake_time_source.h"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "@com_google_absl//absl/time",
    ],
)

envoy_cc_test(
    name = "fake_time_source_test",
    srcs = ["fake_time_source_test.cc"],
    repository = "@envoy",
    deps = [
        ":fake_time_source",
    ],
)

envoy_cc_test(
    name = "nighthawk_service_client_test",
    srcs = ["nighthawk_service_client_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_service_client_impl",
        "//test/test_common:mock_stream",
        "//test/test_common:proto_matchers",
    ],
)

envoy_cc_test(
    name = "signal_handler_test",
    srcs = ["signal_handler_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_common_lib",
    ],
)
