load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "request_options_list_plugin_impl",
    srcs = [
        "request_options_list_plugin_impl.cc",
    ],
    hdrs = [
        "request_options_list_plugin_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/request_source:request_source_plugin_config_factory_lib",
        "//source/common:nighthawk_common_lib",
        "//source/common:request_impl_lib",
        "//source/common:request_source_impl_lib",
        "@envoy//source/common/common:thread_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//source/exe:platform_header_lib_with_external_headers",
        "@envoy//source/exe:platform_impl_lib",
    ],
)
