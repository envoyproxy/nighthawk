load("@rules_python//python:pip.bzl", "pip_parse")
load("@python3_11//:defs.bzl", "interpreter")

def nighthawk_python_dependencies():
    pip_parse(
        name = "python_pip_deps",
        python_interpreter_target = interpreter,
        requirements_lock = "//tools/base:requirements.txt",
        extra_pip_args = ["--require-hashes"],
    )
