load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "sink_test",
    srcs = ["sink_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/sink:sink_impl_lib",
        "@com_github_grpc_grpc//:grpc++_test",  # Avoids undefined symbol _ZN4grpc24g_core_codegen_interfaceE in coverage test build.
        "@envoy//source/common/common:random_generator_lib_with_external_headers",
    ],
)

envoy_cc_test(
    name = "nighthawk_sink_client_test",
    srcs = ["nighthawk_sink_client_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/sink:nighthawk_sink_client_impl",
        "@com_github_grpc_grpc//:grpc++_test",
    ],
)

envoy_cc_test(
    name = "sink_service_test",
    srcs = ["sink_service_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/sink:grpc_service_lib",
        "//test/mocks/sink:mock_sink",
        "//test/test_common:environment_lib",
        "@envoy//test/test_common:network_utility_lib",
    ],
)
