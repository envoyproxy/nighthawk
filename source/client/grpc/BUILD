licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "client_lib",
    srcs = ["client_impl.cc"],
    hdrs = ["client_impl.h"],
    repository = "@envoy",
    deps = [
        "//api/client:base_cc",
        "//api/client:grpc_service_lib",
        "@envoy//include/envoy/grpc:async_client_interface",
        "@envoy//include/envoy/grpc:async_client_manager_interface",
        "@envoy//include/envoy/ratelimit:ratelimit_interface",
        "@envoy//include/envoy/server:filter_config_interface",
        "@envoy//include/envoy/upstream:cluster_manager_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/grpc:typed_async_client_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/tracing:http_tracer_lib",
    ],
)
