load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "adaptive_load_client_main_lib",
    srcs = [
        "adaptive_load_client_main.cc",
    ],
    hdrs = [
        "adaptive_load_client_main.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":adaptive_load_lib",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@com_google_absl//absl/status",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/filesystem:filesystem_lib",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "adaptive_load_lib",
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":adaptive_load_controller_impl_lib",
        ":input_variable_setter_impl_lib",
        ":metrics_plugin_impl_lib",
        ":scoring_function_impl_lib",
        ":step_controller_impl_lib",
    ],
)

envoy_cc_library(
    name = "adaptive_load_controller_impl_lib",
    srcs = [
        "adaptive_load_controller_impl.cc",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":metrics_plugin_impl_lib",
        ":plugin_util_lib",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_load:adaptive_load_controller_include",
        "//include/nighthawk/adaptive_load:metrics_plugin_include",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@com_google_absl//absl/status",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "input_variable_setter_impl_lib",
    srcs = [
        "input_variable_setter_impl.cc",
    ],
    hdrs = [
        "input_variable_setter_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_load:input_variable_setter_include",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@com_google_absl//absl/status",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "metrics_plugin_impl_lib",
    srcs = [
        "metrics_plugin_impl.cc",
    ],
    hdrs = [
        "metrics_plugin_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_load:metrics_plugin_include",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@com_google_absl//absl/status",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "plugin_util_lib",
    srcs = [
        "plugin_util.cc",
    ],
    hdrs = [
        "plugin_util.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_load:input_variable_setter_include",
        "//include/nighthawk/adaptive_load:metrics_plugin_include",
        "//include/nighthawk/adaptive_load:scoring_function_include",
        "//include/nighthawk/adaptive_load:step_controller_include",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@com_google_absl//absl/status",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "scoring_function_impl_lib",
    srcs = [
        "scoring_function_impl.cc",
    ],
    hdrs = [
        "scoring_function_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_load:scoring_function_include",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@com_google_absl//absl/status",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "step_controller_impl_lib",
    srcs = [
        "step_controller_impl.cc",
    ],
    hdrs = [
        "step_controller_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":input_variable_setter_impl_lib",
        ":plugin_util_lib",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//include/nighthawk/adaptive_load:input_variable_setter_include",
        "//include/nighthawk/adaptive_load:step_controller_include",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@com_google_absl//absl/status",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)
