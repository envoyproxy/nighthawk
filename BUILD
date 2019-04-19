load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
)

package(default_visibility = ["//visibility:public"])

envoy_cc_binary(
    name = "nighthawk_client",
    repository = "@envoy",
    deps = [
        "//source/exe:nighthawk_client_entry_lib",
    ],
)

envoy_cc_binary(
    name = "nighthawk_test_server",
    repository = "@envoy",
    deps = [
        "//source/server:http_test_server_filter_config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
