licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_mock(
    name = "nighthawk_mocks",
    srcs = ["mocks.cc"],
    hdrs = ["mocks.h"],
    repository = "@envoy",
    deps = [
        "//include/nighthawk/common:base_includes",
        "//source/client:nighthawk_client_lib",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)

envoy_cc_test(
    name = "nighthawk_test",
    srcs = [
        "benchmark_http_client_test.cc",
        "client_test.cc",
        "client_worker_test.cc",
        "factories_test.cc",
        "frequency_test.cc",
        "options_test.cc",
        "output_collector_test.cc",
        "platform_util_test.cc",
        "pool_test.cc",
        "process_test.cc",
        "rate_limiter_test.cc",
        "sequencer_test.cc",
        "service_test.cc",
        "statistic_test.cc",
        "stopwatch_test.cc",
        "stream_decoder_test.cc",
        "utility_test.cc",
        "worker_test.cc",
    ],
    data = [
        "test_data/benchmark_http_client_test_envoy.yaml",
        "test_data/hdr_proto_json.gold",
        "test_data/lorem_ipsum.txt",
        "test_data/output_collector.json.gold",
        "test_data/output_collector.txt.gold",
        "test_data/output_collector.yaml.gold",
        "@envoy//test/config/integration/certs",
        "@envoy//test/test_common:thread_factory_for_test_lib",
    ],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//test:nighthawk_mocks",
        "//test/client:utility_lib",
        "//test/test_common:environment_lib",
        "@envoy//source/common/filesystem:filesystem_lib",
        "@envoy//source/common/network:dns_lib",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//source/common/stats:isolated_store_lib",
        "@envoy//test/integration:integration_lib",
        "@envoy//test/mocks/event:event_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/server:utility_lib",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)
