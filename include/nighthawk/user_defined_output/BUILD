load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "user_defined_output_plugin",
    hdrs = [
        "user_defined_output_plugin.h",
    ],
    include_prefix = "nighthawk/user_defined_output",
    deps = [
        "@envoy//envoy/common:base_includes",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//source/common/http:header_map_lib_with_external_headers",
    ],
)

envoy_basic_cc_library(
    name = "user_defined_output_plugin_factory",
    hdrs = [
        "user_defined_output_plugin_factory.h",
    ],
    include_prefix = "nighthawk/user_defined_output",
    deps = [
        ":user_defined_output_plugin",
        "@envoy//envoy/common:base_includes",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//envoy/config:typed_config_interface",
    ],
)