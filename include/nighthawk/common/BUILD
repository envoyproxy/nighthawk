load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "base_includes",
    hdrs = [
        "exception.h",
        "factories.h",
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
        "//api/client:base_cc_proto",
        "@envoy//include/envoy/upstream:cluster_manager_interface_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/common:non_copyable_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/event:dispatcher_lib_with_external_headers",
        "@envoy//source/common/network:utility_lib_with_external_headers",
    ],
)

envoy_basic_cc_library(
    name = "nighthawk_service_client",
    hdrs = [
        "nighthawk_service_client.h",
    ],
    include_prefix = "nighthawk/common",
    deps = [
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
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

envoy_basic_cc_library(
    name = "stopwatch_lib",
    hdrs = [
        "stopwatch.h",
    ],
    include_prefix = "nighthawk/common",
    deps = [
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/common:time_interface",
    ],
)
