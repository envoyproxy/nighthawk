load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "envoy_stats_sink_adapter_test",
    srcs = ["envoy_stats_sink_adapter_test.cc"],
    deps = [
        "//api/stats_sink:envoy_stats_sink_adapter_proto_cc_proto",
        "//source/common:nighthawk_common_lib",
        "//source/stats_sink:envoy_stats_sink_adapter",
        "@envoy//source/common/config:well_known_names",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//source/common/stats:allocator_lib_with_external_headers",
        "@envoy//source/common/stats:thread_local_store_lib_with_external_headers",
        "@envoy//source/server:configuration_lib_with_external_headers",
        "@envoy//test/mocks/server:server_factory_context_mocks",
        "@envoy//test/mocks/stats:stats_mocks",
        "@envoy//test/test_common:registry_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
