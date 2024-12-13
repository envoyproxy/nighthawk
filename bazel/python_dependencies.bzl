load("@rules_python//python:pip.bzl", "pip_parse")

def nighthawk_python_dependencies():
    pip_parse(
        name = "nh_pip3",
        python_interpreter_target = "@python3_12_host//:python",
        requirements_lock = "//tools/base:requirements.txt",
        extra_pip_args = ["--require-hashes"],
    )
