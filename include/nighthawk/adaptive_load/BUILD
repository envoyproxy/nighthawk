load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "adaptive_load_controller",
    hdrs = [
        "adaptive_load_controller.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        ":config_validator",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:grpc_service_lib",
        "@envoy//envoy/common:base_includes",
        "@envoy//envoy/common:time_interface",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
    ],
)

envoy_basic_cc_library(
    name = "config_validator",
    hdrs = [
        "config_validator.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "@envoy//envoy/common:base_includes",
        "@envoy//envoy/common:time_interface",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_basic_cc_library(
    name = "input_variable_setter",
    hdrs = [
        "input_variable_setter.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        ":config_validator",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//envoy/common:base_includes",
        "@envoy//envoy/config:typed_config_interface",
    ],
)

envoy_basic_cc_library(
    name = "metrics_evaluator",
    hdrs = [
        "metrics_evaluator.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        ":metrics_plugin",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//envoy/common:base_includes",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
    ],
)

envoy_basic_cc_library(
    name = "metrics_plugin",
    hdrs = [
        "metrics_plugin.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        ":config_validator",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//envoy/common:base_includes",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
    ],
)

envoy_basic_cc_library(
    name = "scoring_function",
    hdrs = [
        "scoring_function.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        ":config_validator",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//envoy/common:base_includes",
        "@envoy//envoy/config:typed_config_interface",
    ],
)

envoy_basic_cc_library(
    name = "session_spec_proto_helper",
    hdrs = [
        "session_spec_proto_helper.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@com_google_absl//absl/status",
        "@envoy//envoy/common:base_includes",
    ],
)

envoy_basic_cc_library(
    name = "step_controller",
    hdrs = [
        "step_controller.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        ":config_validator",
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//envoy/common:base_includes",
        "@envoy//envoy/config:typed_config_interface",
    ],
)
