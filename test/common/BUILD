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
        "@com_github_grpc_grpc//:grpc++_test",
    ],
)

envoy_cc_test(
    name = "nighthawk_sink_client_test",
    srcs = ["nighthawk_sink_client_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_sink_client_impl",
        "@com_github_grpc_grpc//:grpc++_test",
    ],
)

envoy_cc_test(
    name = "sink_test",
    srcs = ["sink_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:sink_impl_lib",
        "@com_github_grpc_grpc//:grpc++_test",  # Avoids undefined symbol _ZN4grpc24g_core_codegen_interfaceE in coverage test build.
        "@envoy//source/common/common:random_generator_lib_with_external_headers",
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
