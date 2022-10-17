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
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//envoy/common:base_includes",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/http:header_map_lib_with_external_headers",
    ],
)
