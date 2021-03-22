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
        "@com_github_grpc_grpc//:grpc++_test",
    ],
)
