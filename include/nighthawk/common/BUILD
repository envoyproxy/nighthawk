licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

envoy_package()

envoy_basic_cc_library(
    name = "base_includes",
    hdrs = [
        "exception.h",
        "operation_callback.h",
        "phase.h",
        "platform_util.h",
        "rate_limiter.h",
        "request_source.h",
        "request_stream_grpc_client.h",
        "sequencer.h",
        "statistic.h",
        "termination_predicate.h",
        "uri.h",
        "worker.h",
    ],
    include_prefix = "nighthawk/common",
    deps = [
        ":request_lib",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/common:non_copyable",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/network:dns_lib",
        "@envoy//source/common/protobuf",
    ],
)

envoy_basic_cc_library(
    name = "request_lib",
    hdrs = [
        "request.h",
    ],
    include_prefix = "nighthawk/common",
    deps = [
        "@envoy//source/common/http:headers_lib",
    ],
)

envoy_basic_cc_library(
    name = "request_source_lib",
    hdrs = [
        "request_source.h",
    ],
    include_prefix = "nighthawk/common",
    deps = [
        ":request_lib",
        "@envoy//source/common/http:headers_lib",
    ],
)
