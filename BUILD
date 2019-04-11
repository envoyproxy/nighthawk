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
