load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

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

envoy_cc_library(
    name = "llm_request_source_plugin_impl",
    srcs = [
        "llm_request_source_plugin_impl.cc",
    ],
    hdrs = [
        "llm_request_source_plugin_impl.h",
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
	"@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
	"@com_google_absl//absl/strings:string_view",
    ],
)

api_cc_py_proto_library(
    name = "llm_request_source_plugin_proto",
    srcs = [
        "llm_request_source_plugin.proto",
    ],
    deps = [
	"//api/client:base",
    ],
)
