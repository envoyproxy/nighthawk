load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "nighthawk_distributor_client_test",
    srcs = ["nighthawk_distributor_client_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/distributor:nighthawk_distributor_client_impl",
        "//test/test_common:mock_stream",
        "//test/test_common:proto_matchers",
    ],
)

envoy_cc_test(
    name = "distributor_service_test",
    srcs = ["distributor_service_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_service_client_impl",
        "//source/distributor:grpc_service_lib",
        "//test/mocks/common:mock_nighthawk_service_client",
        "//test/test_common:environment_lib",
        "@envoy//test/test_common:network_utility_lib",
    ],
)
