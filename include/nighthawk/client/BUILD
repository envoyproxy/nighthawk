licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

envoy_package()

envoy_basic_cc_library(
    name = "client_includes",
    hdrs = [
        "benchmark_client.h",
        "client_worker.h",
        "factories.h",
        "options.h",
        "output_collector.h",
        "process.h",
    ],
    include_prefix = "nighthawk/client",
    deps = [
        "//include/nighthawk/common:base_includes",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/http:conn_pool_interface_with_external_headers",
        "@envoy//source/common/api:api_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/common:non_copyable_with_external_headers",
        "@envoy//source/common/network:dns_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
        "@envoy//source/common/runtime:runtime_lib_with_external_headers",
        "@envoy_api//envoy/api/v2:cds_export_cc",
    ],
)
