load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "nighthawk_client_lib",
    srcs = [
        "benchmark_client_impl.cc",
        "client.cc",
        "client_worker_impl.cc",
        "factories_impl.cc",
        "flush_worker_impl.cc",
        "options_impl.cc",
        "process_impl.cc",
        "remote_process_impl.cc",
        "sni_utility.cc",
        "stream_decoder.cc",
    ],
    hdrs = [
        "benchmark_client_impl.h",
        "client.h",
        "client_worker_impl.h",
        "factories_impl.h",
        "flush_worker_impl.h",
        "options_impl.h",
        "process_impl.h",
        "remote_process_impl.h",
        "sni_utility.h",
        "stream_decoder.h",
    ],
    copts = select({
        "//bazel:zipkin_disabled": [],
        "//conditions:default": ["-DZIPKIN_ENABLED=1"],
    }),
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":output_collector_impl_lib",
        ":output_formatter_impl_lib",
        "//api/client:base_cc_proto",
        "//include/nighthawk/client:client_includes",
        "//include/nighthawk/common:base_includes",
        "//source/common:request_source_impl_lib",
        "//source/common:nighthawk_common_lib",
        "//source/common:nighthawk_service_client_impl",
        "//source/request_source:request_options_list_plugin_impl",
        "@envoy//source/common/common:random_generator_lib_with_external_headers",
        "@envoy//source/common/access_log:access_log_manager_lib_with_external_headers",
        "@envoy//source/common/api:api_lib_with_external_headers",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:dispatcher_includes_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:context_lib_with_external_headers",
        "@envoy//source/common/http:context_lib_with_external_headers",
        "@envoy//source/common/http:header_map_lib_with_external_headers",
        "@envoy//source/common/http:headers_lib_with_external_headers",
        "@envoy//source/common/http/http1:codec_lib_with_external_headers",
        "@envoy//source/common/http/http1:conn_pool_lib_with_external_headers",
        "@envoy//source/common/http/http2:conn_pool_lib_with_external_headers",
        "@envoy//source/common/init:manager_lib_with_external_headers",
        "@envoy//source/common/local_info:local_info_lib_with_external_headers",
        "@envoy//source/common/network:dns_lib_with_external_headers",
        "@envoy//source/common/network:address_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//source/common/runtime:runtime_lib_with_external_headers",
        "@envoy//source/common/secret:secret_manager_impl_lib_with_external_headers",
        "@envoy//source/common/singleton:manager_impl_lib_with_external_headers",
        "@envoy//source/common/stats:allocator_lib_with_external_headers",
        "@envoy//source/common/stats:symbol_table_lib_with_external_headers",
        "@envoy//source/common/stats:thread_local_store_lib_with_external_headers",
        "@envoy//source/common/stream_info:stream_info_lib_with_external_headers",
        "@envoy//source/common/thread_local:thread_local_lib_with_external_headers",
        "@envoy//source/common/tracing:http_tracer_lib_with_external_headers",
        "@envoy//source/common/upstream:cluster_manager_lib_with_external_headers",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
        "@envoy//source/exe:platform_header_lib_with_external_headers",
        "@envoy//source/exe:platform_impl_lib",
        "@envoy//source/exe:process_wide_lib_with_external_headers",
        "@envoy//source/common/http:request_id_extension_lib_with_external_headers",
        "@envoy//source/extensions/transport_sockets:well_known_names_with_external_headers",
        "@envoy//source/extensions/transport_sockets/tls:context_lib_with_external_headers",
        "@envoy//source/server:options_lib_with_external_headers",
        "@envoy//source/server:server_lib_with_external_headers",
        "@envoy//source/server/config_validation:admin_lib_with_external_headers",
        "@envoy//include/envoy/http:protocol_interface_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
    ] + select({
        "//bazel:zipkin_disabled": [],
        "//conditions:default": [
            "@envoy//source/extensions/tracers/zipkin:zipkin_lib_with_external_headers",
        ],
    }),
)

envoy_cc_library(
    name = "output_collector_impl_lib",
    srcs = [
        "output_collector_impl.cc",
    ],
    hdrs = [
        "output_collector_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base_cc_proto",
        "//include/nighthawk/client:client_includes",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "output_formatter_impl_lib",
    srcs = [
        "output_formatter_impl.cc",
    ],
    hdrs = [
        "output_formatter_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base_cc_proto",
        "//api/client/transform:transform_cc_proto",
        "//include/nighthawk/client:client_includes",
        "//source/common:nighthawk_common_lib",
        "@envoy//source/common/network:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "nighthawk_service_lib",
    srcs = [
        "service_impl.cc",
        "service_main.cc",
    ],
    hdrs = [
        "service_impl.h",
        "service_main.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":nighthawk_client_lib",
        "//api/client:grpc_service_lib",
        "//api/request_source:grpc_request_source_service_lib",
        "@envoy//source/common/common:thread_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "output_transform_main_lib",
    srcs = [
        "output_transform_main.cc",
    ],
    hdrs = [
        "output_transform_main.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":nighthawk_client_lib",
        ":output_collector_impl_lib",
        "//source/common:nighthawk_common_lib",
    ],
)
