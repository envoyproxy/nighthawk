load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "adaptive_load_controller_impl",
    srcs = [
        "adaptive_load_controller_impl.cc",
    ],
    hdrs = [
        "adaptive_load_controller_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":metrics_plugin_impl",
        ":plugin_loader",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//include/nighthawk/adaptive_load:adaptive_load_controller",
        "//include/nighthawk/adaptive_load:metrics_evaluator",
        "//include/nighthawk/adaptive_load:scoring_function",
        "//include/nighthawk/adaptive_load:session_spec_proto_helper",
        "//include/nighthawk/adaptive_load:step_controller",
        "//include/nighthawk/common:nighthawk_service_client",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "adaptive_load_client_main",
    srcs = [
        "adaptive_load_client_main.cc",
    ],
    hdrs = [
        "adaptive_load_client_main.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":adaptive_load_controller_impl",
        ":config_validator_impl",
        ":input_variable_setter_impl",
        ":metrics_evaluator_impl",
        ":metrics_plugin_impl",
        ":scoring_function_impl",
        ":session_spec_proto_helper_impl",
        ":step_controller_impl",
        "//api/client:base_cc_proto",
        "//api/client:grpc_service_lib",
        "//source/common:nighthawk_common_lib",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:google_grpc_utils_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "config_validator_impl",
    srcs = [
        "config_validator_impl.cc",
    ],
    hdrs = [
        "config_validator_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/adaptive_load:config_validator",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_cc_library(
    name = "input_variable_setter_impl",
    srcs = [
        "input_variable_setter_impl.cc",
    ],
    hdrs = [
        "input_variable_setter_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/adaptive_load:input_variable_setter",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_cc_library(
    name = "metrics_evaluator_impl",
    srcs = [
        "metrics_evaluator_impl.cc",
    ],
    hdrs = [
        "metrics_evaluator_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":metrics_plugin_impl",
        ":plugin_loader",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:base_cc_proto",
        "//include/nighthawk/adaptive_load:adaptive_load_controller",
        "//include/nighthawk/adaptive_load:metrics_evaluator",
        "//include/nighthawk/adaptive_load:scoring_function",
    ],
)

envoy_cc_library(
    name = "metrics_plugin_impl",
    srcs = [
        "metrics_plugin_impl.cc",
    ],
    hdrs = [
        "metrics_plugin_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/adaptive_load:metrics_plugin",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_cc_library(
    name = "plugin_loader",
    srcs = [
        "plugin_loader.cc",
    ],
    hdrs = [
        "plugin_loader.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/adaptive_load:input_variable_setter",
        "//include/nighthawk/adaptive_load:metrics_plugin",
        "//include/nighthawk/adaptive_load:scoring_function",
        "//include/nighthawk/adaptive_load:step_controller",
        "//source/common:nighthawk_common_lib",
        "@envoy//source/common/config:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "scoring_function_impl",
    srcs = [
        "scoring_function_impl.cc",
    ],
    hdrs = [
        "scoring_function_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/adaptive_load:scoring_function",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_cc_library(
    name = "session_spec_proto_helper_impl",
    srcs = [
        "session_spec_proto_helper_impl.cc",
    ],
    hdrs = [
        "session_spec_proto_helper_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":metrics_plugin_impl",
        ":plugin_loader",
        "//include/nighthawk/adaptive_load:metrics_plugin",
        "//include/nighthawk/adaptive_load:scoring_function",
        "//include/nighthawk/adaptive_load:session_spec_proto_helper",
        "//include/nighthawk/adaptive_load:step_controller",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_cc_library(
    name = "step_controller_impl",
    srcs = [
        "step_controller_impl.cc",
    ],
    hdrs = [
        "step_controller_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":input_variable_setter_impl",
        ":plugin_loader",
        "//include/nighthawk/adaptive_load:input_variable_setter",
        "//include/nighthawk/adaptive_load:step_controller",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)
