load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "nighthawk_sink_client",
    hdrs = [
        "nighthawk_sink_client.h",
    ],
    include_prefix = "nighthawk/sink",
    deps = [
        "//api/sink:sink_cc_proto",
        "//api/sink:sink_grpc_lib",
        "@envoy//envoy/common:base_includes",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_basic_cc_library(
    name = "sink_lib",
    hdrs = [
        "sink.h",
    ],
    include_prefix = "nighthawk/sink",
    deps = [
        "//api/client:grpc_service_lib",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
    ],
)
