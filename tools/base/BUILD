load("@rules_python//python:pip.bzl", "compile_pip_requirements")
load("@envoy//bazel:envoy_build_system.bzl", "envoy_package")

licenses(["notice"])  # Apache 2

envoy_package()

compile_pip_requirements(
    name = "requirements",
    src = "requirements.in",
    extra_args = [
        "--generate-hashes",
        "--reuse-hashes",
    ],
)
