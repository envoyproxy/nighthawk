load("@python_pip_deps//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_binary")

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

py_binary(
    name = "format_python_tools",
    srcs = ["format_python_tools.py"],
    data = [
        ":.style.yapf",
    ],
    deps = [
        requirement("flake8"),
        requirement("flake8-docstrings"),
        requirement("mccabe"),
        requirement("pycodestyle"),
        requirement("pydocstyle"),
        requirement("pyflakes"),
        requirement("snowballstemmer"),
        requirement("yapf"),
    ],
)
