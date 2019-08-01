licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load("@python_pip_deps//:requirements.bzl", "requirement")

envoy_package()

py_library(
    name = "integration_test_base",
    srcs = [
        "common.py",
        "integration_test_fixtures.py",
        "nighthawk_test_server.py",
    ],
    data = [
        "configurations/nighthawk_http_origin.yaml",
        "configurations/nighthawk_https_origin.yaml",
        "//:nighthawk_client",
        "//:nighthawk_test_server",
        "@envoy//test/config/integration/certs",
    ],
    deps = [
        requirement("requests"),
        # The following are implied by 'request'.
        requirement("urllib3"),
        requirement("chardet"),
        requirement("certifi"),
        requirement("idna"),
    ],
)

py_binary(
    name = "cpp_benchmark_client_server",
    srcs = [
        "cpp_benchmark_client_server.py",
    ],
    deps = [
        ":integration_test_base",
    ],
)

py_test(
    name = "integration_test",
    srcs = [
        "integration_test.py",
        "test_integration_basics.py",
    ],
    deps = [
        ":integration_test_base",
    ],
)
