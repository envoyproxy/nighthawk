load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "benchmark_http_client_test",
    srcs = ["benchmark_http_client_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//source/common:request_impl_lib",
        "//test/test_common:environment_lib",
        "@envoy//source/common/http:header_map_lib_with_external_headers",
        "@envoy//source/common/network:utility_lib_with_external_headers",
        "@envoy//source/common/stats:isolated_store_lib_with_external_headers",
        "@envoy//source/exe:process_wide_lib_with_external_headers",
        "@envoy//test/mocks/runtime:runtime_mocks",
        "@envoy//test/mocks/stream_info:stream_info_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:network_utility_lib",
    ],
)

envoy_cc_test(
    name = "client_test",
    srcs = ["client_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//test/client:utility_lib",
        "//test/test_common:environment_lib",
    ],
)

envoy_cc_test(
    name = "client_worker_test",
    srcs = ["client_worker_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//test/mocks/client:mock_benchmark_client",
        "//test/mocks/client:mock_benchmark_client_factory",
        "//test/mocks/common:mock_request_source",
        "//test/mocks/common:mock_request_source_factory",
        "//test/mocks/common:mock_sequencer",
        "//test/mocks/common:mock_sequencer_factory",
        "//test/mocks/common:mock_termination_predicate",
        "//test/mocks/common:mock_termination_predicate_factory",
        "@envoy//source/common/api:api_lib",
        "@envoy//source/common/stats:isolated_store_lib_with_external_headers",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/protobuf:protobuf_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)

envoy_cc_test(
    name = "flush_worker_test",
    srcs = ["flush_worker_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "@envoy//source/common/api:api_lib",
        "@envoy//source/common/event:dispatcher_includes_with_external_headers",
        "@envoy//source/common/stats:isolated_store_lib_with_external_headers",
        "@envoy//test/mocks/api:api_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/protobuf:protobuf_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
    ],
)

envoy_cc_test(
    name = "factories_test",
    srcs = ["factories_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//test/mocks/client:mock_benchmark_client",
        "//test/mocks/client:mock_options",
        "//test/mocks/common:mock_termination_predicate",
        "//test/test_common:environment_lib",
        "@envoy//test/mocks/event:event_mocks",
        "@envoy//test/mocks/tracing:tracing_mocks",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)

envoy_cc_test(
    name = "frequency_test",
    srcs = ["frequency_test.cc"],
    repository = "@envoy",
    deps = ["//source/common:nighthawk_common_lib"],
)

envoy_cc_test(
    name = "options_test",
    srcs = ["options_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//test/client:utility_lib",
        "//test/test_common:environment_lib",
    ],
)

envoy_cc_test(
    name = "output_formatter_test",
    srcs = ["output_formatter_test.cc"],
    data = [
        "test_data/output_formatter.dotted.gold",
        "test_data/output_formatter.json.gold",
        "test_data/output_formatter.medium.fortio.gold",
        "test_data/output_formatter.medium.fortio-noquirks.gold",
        "test_data/output_formatter.medium.proto.gold",
        "test_data/output_formatter.txt.gold",
        "test_data/output_formatter.yaml.gold",
        "test_data/percentile-column-overflow.json",
        "test_data/percentile-column-overflow.txt.gold",
    ],
    repository = "@envoy",
    deps = [
        "//source/client:output_collector_impl_lib",
        "//source/client:output_formatter_impl_lib",
        "//source/common:nighthawk_common_lib",
        "//test/mocks/client:mock_options",
        "//test/test_common:environment_lib",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)

envoy_cc_test(
    name = "platform_util_test",
    srcs = ["platform_util_test.cc"],
    repository = "@envoy",
    deps = ["//source/common:nighthawk_common_lib"],
)

envoy_cc_test(
    name = "process_test",
    srcs = ["process_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//test/client:utility_lib",
        "//test/test_common:environment_lib",
        "@envoy//test/test_common:network_utility_lib",
        "@envoy//test/test_common:registry_lib",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)

envoy_cc_test(
    name = "rate_limiter_test",
    srcs = ["rate_limiter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_common_lib",
        "//test/mocks/common:mock_rate_limiter",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)

envoy_cc_test(
    name = "sequencer_test",
    srcs = ["sequencer_test.cc"],
    data = [
        "test_data/hdr_proto_json.gold",
        "test_data/output_formatter.json.gold",
        "test_data/output_formatter.txt.gold",
        "test_data/output_formatter.yaml.gold",
    ],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_common_lib",
        "//test/mocks/common:mock_platform_util",
        "//test/mocks/common:mock_rate_limiter",
        "//test/mocks/common:mock_termination_predicate",
        "@envoy//source/common/event:dispatcher_includes_with_external_headers",
        "@envoy//source/common/stats:isolated_store_lib_with_external_headers",
        "@envoy//test/mocks/event:event_mocks",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)

envoy_cc_test(
    name = "service_main_test",
    srcs = ["service_main_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_service_lib",
        "//test/test_common:environment_lib",
    ],
)

envoy_cc_test(
    name = "service_test",
    srcs = ["service_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_service_lib",
        "//test/test_common:environment_lib",
        "@envoy//test/test_common:network_utility_lib",
    ],
)

envoy_cc_test(
    name = "statistic_test",
    srcs = ["statistic_test.cc"],
    data = [
        "test_data/circllhist_proto_json.gold",
        "test_data/hdr_proto_json.gold",
    ],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_common_lib",
        "//test/test_common:environment_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//source/common/stats:isolated_store_lib_with_external_headers",
        "@envoy//test/mocks/stats:stats_mocks",
    ],
)

envoy_cc_test(
    name = "stream_decoder_test",
    srcs = ["stream_decoder_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "@envoy//source/common/event:dispatcher_includes_with_external_headers",
        "@envoy//source/common/http:header_map_lib_with_external_headers",
        "@envoy//source/common/network:utility_lib_with_external_headers",
        "@envoy//source/common/stats:isolated_store_lib_with_external_headers",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/stream_info:stream_info_mocks",
    ],
)

envoy_cc_test(
    name = "utility_test",
    srcs = ["utility_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_common_lib",
        "//test/test_common:environment_lib",
        "@envoy//source/common/network:utility_lib_with_external_headers",
        "@envoy//source/common/stats:isolated_store_lib_with_external_headers",
    ],
)

envoy_cc_test(
    name = "worker_test",
    srcs = ["worker_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_common_lib",
        "@envoy//source/common/common:random_generator_lib_with_external_headers",
        "@envoy//source/common/stats:isolated_store_lib_with_external_headers",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/protobuf:protobuf_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
    ],
)

envoy_cc_test(
    name = "request_generator_test",
    srcs = ["request_generator_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//test/client:utility_lib",
    ],
)

# We wrap the python integration tests here so we run the instrumented
# code in coverage tests. To make that work, we need to declare the top-level
# libraries as dependencies.
envoy_cc_test(
    name = "python_test",
    srcs = ["python_test.cc"],
    data = ["//test/integration:integration_test"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "//source/client:nighthawk_service_lib",
        "//source/client:output_transform_main_lib",
        "//source/common:nighthawk_common_lib",
        "//source/server:http_test_server_filter_lib",
        "//test/test_common:environment_lib",
        "@envoy//test/test_common:network_utility_lib",
    ],
)

envoy_cc_test(
    name = "output_transform_main_test",
    srcs = ["output_transform_main_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:output_transform_main_lib",
        "//test/test_common:environment_lib",
        "@envoy//test/test_common:network_utility_lib",
    ],
)

envoy_cc_test(
    name = "termination_predicate_test",
    srcs = ["termination_predicate_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:nighthawk_common_lib",
        "@envoy//test/mocks/event:event_mocks",
        "@envoy//test/test_common:simulated_time_system_lib",
    ],
)

envoy_cc_test(
    name = "request_stream_grpc_client_test",
    srcs = ["request_stream_grpc_client_test.cc"],
    repository = "@envoy",
    deps = [
        "//api/request_source:grpc_request_source_service_lib",
        "//source/common:request_stream_grpc_client_lib",
        "@envoy//test/test_common:utility_lib",
        "@envoy_api//envoy/api/v2/core:pkg_cc_proto",
        "@envoy_api//envoy/config/core/v3:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "sni_utility_test",
    srcs = ["sni_utility_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/client:nighthawk_client_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "stopwatch_test",
    srcs = ["stopwatch_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:thread_safe_monotonic_time_stopwatch_lib",
        "//test/common:fake_time_source",
        "@envoy//test/test_common:simulated_time_system_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
