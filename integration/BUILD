licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load("@python_pip_deps//:requirements.bzl", "requirement")

envoy_package()

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
    # We enforce python 2, but I we may be able to stop doing that when we update
    # bazel in CI. (Or we add python3 explicitly as a build dependency somehow).
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
