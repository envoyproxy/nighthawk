licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "nighthawk_client_lib",
    srcs = [
        "benchmark_client_impl.cc",
        "client.cc",
        "client_worker_impl.cc",
        "factories_impl.cc",
        "options_impl.cc",
        "output_collector_impl.cc",
        "process_impl.cc",
        "stream_decoder.cc",
    ],
    hdrs = [
        "benchmark_client_impl.h",
        "client.h",
        "client_worker_impl.h",
        "factories_impl.h",
        "options_impl.h",
        "output_collector_impl.h",
        "process_impl.h",
        "stream_decoder.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:benchmark_options_cc",
        "//include/nighthawk/client:client_includes",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@envoy//source/common/common:compiler_requirements_lib",
        "@envoy//source/common/common:thread_impl_lib_posix",
        "@envoy//source/common/filesystem:filesystem_lib",
        "@envoy//source/common/network:dns_lib",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//source/common/runtime:runtime_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//source/exe:process_wide_lib",
    ],
)
