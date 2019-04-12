load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

licenses(["notice"])  # Apache 2

envoy_cc_library(
    name = "nighthawk_client_entry_lib",
    srcs = ["client_main_entry.cc"],
    external_deps = [
        "abseil_symbolize",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//nighthawk/source/client:nighthawk_client_lib",
        "@envoy//source/common/common:thread_lib",
    ],
)
