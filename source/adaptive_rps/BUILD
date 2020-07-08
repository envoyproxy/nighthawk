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
        "adaptive_rps_client_main.cc",
        "adaptive_rps_controller_impl.cc",
        "metrics_plugin_impl.cc",
        "plugin_util.cc",
        "scoring_function_impl.cc",
        "step_controller_impl.cc",
    ],
    hdrs = [
        "adaptive_rps_client_main.h",
        "metrics_plugin_impl.h",
        "plugin_util.h",
        "scoring_function_impl.h",
        "step_controller_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/adaptive_rps:base_cc_proto",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_rps:adaptive_rps_includes",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@com_google_absl//absl/status",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)
