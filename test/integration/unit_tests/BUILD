load("@rules_python//python:defs.bzl", "py_binary", "py_library")
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load("@python_pip_deps//:requirements.bzl", "requirement")

licenses(["notice"])  # Apache 2

envoy_package()

py_test(
    name = "test_nighthawk_test_server",
    srcs = ["test_nighthawk_test_server.py"],
    deps = [
        "//test/integration:integration_test_base_lean",
    ],
)
