licenses(["notice"])  # Apache 2

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
)
