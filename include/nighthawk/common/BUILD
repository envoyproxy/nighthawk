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
        "platform_util.h",
        "poolable.h",
        "rate_limiter.h",
        "sequencer.h",
        "statistic.h",
        "stopwatch.h",
        "uri.h",
        "worker.h",
    ],
    include_prefix = "nighthawk/common",
    deps = [
        "@envoy//include/envoy/common:base_includes",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/common:non_copyable",
        "@envoy//source/common/network:dns_lib",
        "@envoy//source/common/protobuf",
    ],
)
