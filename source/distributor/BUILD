load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "nighthawk_distributor_client_impl",
    srcs = [
        "nighthawk_distributor_client_impl.cc",
    ],
    hdrs = [
        "nighthawk_distributor_client_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/distributor:nighthawk_distributor_client",
    ],
)
