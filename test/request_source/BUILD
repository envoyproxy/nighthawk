load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "request_source_plugin_test",
    srcs = ["request_source_plugin_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common:request_source_plugin_impl",
    ],
)
