licenses(["notice"])  # Apache 2

exports_files([
    "check_envoy_includes.py",
])

py_binary(
    name = "update_cli_readme_documentation",
    srcs = ["update_cli_readme_documentation.py"],
    data = [
        "//:nighthawk_client",
        "//:nighthawk_output_transform",
        "//:nighthawk_service",
        "//:nighthawk_test_server",
    ],
)
