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
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "//api/client:grpc_service_lib",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/common:time_interface",
        "@envoy//include/envoy/config:typed_config_interface",
    ],
)

envoy_basic_cc_library(
    name = "input_variable_setter",
    hdrs = [
        "input_variable_setter.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/config:typed_config_interface",
    ],
)

envoy_basic_cc_library(
    name = "metrics_plugin",
    hdrs = [
        "metrics_plugin.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/config:typed_config_interface",
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
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/config:typed_config_interface",
    ],
)

envoy_basic_cc_library(
    name = "step_controller",
    hdrs = [
        "step_controller.h",
    ],
    include_prefix = "nighthawk/adaptive_load",
    deps = [
        "//api/adaptive_load:adaptive_load_proto_cc_proto",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/config:typed_config_interface",
    ],
)
