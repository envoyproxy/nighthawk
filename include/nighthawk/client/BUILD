load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "options_lib",
    hdrs = [
        "options.h",
    ],
    include_prefix = "nighthawk/client",
    deps = [
        "//api/client:base_cc_proto",
        "//include/nighthawk/common:base_includes",
        "@envoy//include/envoy/common:time_interface",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy_api//envoy/config/cluster/v3:pkg_cc_proto",
        "@envoy_api//envoy/config/metrics/v3:pkg_cc_proto",
    ],
)

envoy_basic_cc_library(
    name = "client_includes",
    hdrs = [
        "benchmark_client.h",
        "client_worker.h",
        "factories.h",
        "process.h",
    ],
    include_prefix = "nighthawk/client",
    deps = [
        ":options_lib",
        ":output_collector_lib",
        ":output_formatter_lib",
        "//api/client:base_cc_proto",
        "//include/nighthawk/common:base_includes",
        "//include/nighthawk/common:request_source_lib",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/http:conn_pool_interface_with_external_headers",
        "@envoy//source/common/api:api_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/common:non_copyable_with_external_headers",
        "@envoy//source/common/network:dns_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
        "@envoy//source/common/runtime:runtime_lib_with_external_headers",
        "@envoy_api//envoy/api/v2:pkg_cc_proto",
        "@envoy_api//envoy/config/metrics/v3:pkg_cc_proto",
    ],
)

envoy_basic_cc_library(
    name = "output_collector_lib",
    hdrs = ["output_collector.h"],
    include_prefix = "nighthawk/client",
    deps = [
        "//api/client:base_cc_proto",
        "//include/nighthawk/common:base_includes",
        "@envoy//include/envoy/common:base_includes",
    ],
)

envoy_basic_cc_library(
    name = "output_formatter_lib",
    hdrs = ["output_formatter.h"],
    include_prefix = "nighthawk/client",
    deps = [
        "//api/client:base_cc_proto",
        "@envoy//include/envoy/common:base_includes",
    ],
)
