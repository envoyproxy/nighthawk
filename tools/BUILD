licenses(["notice"])  # Apache 2

exports_files([
    "check_envoy_includes.py",
])

py_library(
    name = "tools_lib",
    srcs = [
    ],
    data = [
        "//:nighthawk_client",
    ],
)

py_binary(
    name = "update_cli_readme_documentation",
    srcs = ["update_cli_readme_documentation.py"],
    deps = [
        ":tools_lib",
    ],
)
