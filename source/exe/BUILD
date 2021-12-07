load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "adaptive_load_client_entry_lib",
    srcs = ["adaptive_load_client_main_entry.cc"],
    external_deps = [
        "abseil_symbolize",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//source/adaptive_load:adaptive_load_client_main",
        "//source/common:nighthawk_service_client_impl",
        "//source/common:version_linkstamp",
        "@envoy//source/exe:platform_header_lib_with_external_headers",
        "@envoy//source/exe:platform_impl_lib",
    ],
)

envoy_cc_library(
    name = "nighthawk_client_entry_lib",
    srcs = ["client_main_entry.cc"],
    external_deps = [
        "abseil_symbolize",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//source/client:nighthawk_client_lib",
        "//source/common:version_linkstamp",
    ],
)

envoy_cc_library(
    name = "nighthawk_service_entry_lib",
    srcs = ["service_main_entry.cc"],
    external_deps = [
        "abseil_symbolize",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//source/client:nighthawk_service_lib",
        "//source/common:version_linkstamp",
    ],
)

envoy_cc_library(
    name = "output_transform_main_entry_lib",
    srcs = ["output_transform_main_entry.cc"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//source/client:output_transform_main_lib",
        "//source/common:version_linkstamp",
    ],
)
