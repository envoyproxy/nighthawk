load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "user_defined_output_plugin_creator",
    srcs = [
        "user_defined_output_plugin_creator.cc",
    ],
    hdrs = [
        "user_defined_output_plugin_creator.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/user_defined_output:user_defined_output_plugin",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "log_response_headers_plugin",
    srcs = [
        "log_response_headers_plugin.cc",
    ],
    hdrs = [
        "log_response_headers_plugin.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/user_defined_output:log_response_headers_proto_cc_proto",
        "//include/nighthawk/user_defined_output:user_defined_output_plugin",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
    ],
)
