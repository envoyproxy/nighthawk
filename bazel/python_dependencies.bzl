load("@rules_python//python:pip.bzl", "pip_parse")
load("@python3_11//:defs.bzl", "interpreter")

def nighthawk_python_dependencies():
    pip_parse(
        name = "nh_pip3",
        python_interpreter_target = interpreter,
        requirements_lock = "//tools/base:requirements.txt",
        extra_pip_args = ["--require-hashes"],
    )
