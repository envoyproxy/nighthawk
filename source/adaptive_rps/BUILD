load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "adaptive_rps_lib",
    srcs = [
        "adaptive_rps_controller.cc",
        "custom_metric_evaluator_impl.cc",
        "metrics_plugin_impl.cc",
        "plugin_util.cc",
        "step_controller_impl.cc",
    ],
    hdrs = [
        "adaptive_rps_controller.h",
        "custom_metric_evaluator_impl.h",
        "metrics_plugin_impl.h",
        "plugin_util.h",
        "step_controller_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base_cc_proto",
        "//api/adaptive_rps:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_rps:adaptive_rps_includes",
        "@com_google_absl//absl/status",

        # "//include/nighthawk/common:base_includes",
        # "//source/common:nighthawk_common_lib",
        # "@envoy//source/common/api:api_lib_with_external_headers",
        # "@envoy//source/common/common:cleanup_lib_with_external_headers",
        # "@envoy//source/common/config:utility_lib_with_external_headers",
        # "@envoy//source/common/event:dispatcher_includes_with_external_headers",
        # "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        # "@envoy//source/common/grpc:context_lib_with_external_headers",
        # "@envoy//source/common/http:context_lib_with_external_headers",
        # "@envoy//source/common/http:headers_lib_with_external_headers",
        # "@envoy//source/common/init:manager_lib_with_external_headers",
        # "@envoy//source/common/local_info:local_info_lib_with_external_headers",
        # "@envoy//source/common/network:dns_lib_with_external_headers",
        # "@envoy//source/common/network:address_lib_with_external_headers",
        # "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        # "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        # "@envoy//source/common/runtime:runtime_lib_with_external_headers",
        # "@envoy//source/common/secret:secret_manager_impl_lib_with_external_headers",
        # "@envoy//source/common/singleton:manager_impl_lib_with_external_headers",
        # "@envoy//source/common/stats:allocator_lib_with_external_headers",
        # "@envoy//source/common/stats:symbol_table_lib_with_external_headers",
        # "@envoy//source/common/stats:thread_local_store_lib_with_external_headers",
        # "@envoy//source/common/stream_info:stream_info_lib_with_external_headers",
        # "@envoy//source/common/thread_local:thread_local_lib_with_external_headers",
        # "@envoy//source/common/tracing:http_tracer_lib_with_external_headers",
        # "@envoy//source/common/upstream:cluster_manager_lib_with_external_headers",
        # "@envoy//source/exe:envoy_common_lib_with_external_headers",
        # "@envoy//source/exe:platform_header_lib_with_external_headers",
        # "@envoy//source/exe:platform_impl_lib",
        # "@envoy//source/exe:process_wide_lib_with_external_headers",
        # "@envoy//source/extensions/tracers:well_known_names_with_external_headers",
        # "@envoy//source/server:options_lib_with_external_headers",
        # "@envoy//source/server/config_validation:admin_lib_with_external_headers",
        # "@envoy//include/envoy/http:protocol_interface_with_external_headers",

        "@envoy//source/common/config:utility_lib_with_external_headers",
    ],
)