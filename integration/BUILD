load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load("@python_pip_deps//:requirements.bzl", "requirement")

licenses(["notice"])  # Apache 2

envoy_package()

exports_files(
    [
        "cpp_benchmark_client_server.py",
        "test_integration_basics.py",
        "common.py",
        "integration_test_fixtures.py",
        "nighthawk_test_server.py",
        "integration_test.py",
        "configurations/nighthawk_http_origin.yaml",
        "configurations/nighthawk_https_origin.yaml",
    ],
)

py_test(
    name = "integration_test",
    srcs = [
        "integration_test.py",
    ],
    data = [
        "configurations/nighthawk_http_origin.yaml",
        "configurations/nighthawk_https_origin.yaml",
        "//:nighthawk_client",
        "//:nighthawk_test_server",
        "@envoy//test/config/integration/certs",
    ],
    python_version = "PY3",
    srcs_version = "PY3ONLY",
    deps = [
        requirement("requests"),
        # The following are implied by 'request'.
        requirement("urllib3"),
        requirement("chardet"),
        requirement("certifi"),
        requirement("idna"),
    ],
)
