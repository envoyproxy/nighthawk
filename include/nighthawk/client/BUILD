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
        "output_formatter.h",
        "prefetchable_pool.h",
        "process_context.h",
    ],
    include_prefix = "nighthawk/client",
    deps = [
        "//include/nighthawk/common:base_includes",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/http:conn_pool_interface",
        "@envoy//source/common/api:api_lib",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/common:non_copyable",
        "@envoy//source/common/network:dns_lib",
        "@envoy//source/common/protobuf",
        "@envoy//source/common/runtime:runtime_lib",
    ],
)
