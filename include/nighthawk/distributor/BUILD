load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "nighthawk_distributor_client",
    hdrs = [
        "nighthawk_distributor_client.h",
    ],
    include_prefix = "nighthawk/common",
    deps = [
        "//api/distributor:distributor_cc_proto",
        "//api/distributor:distributor_grpc_lib",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)
