workspace(name = "nighthawk")

load("//bazel:repositories.bzl", "nighthawk_dependencies")

nighthawk_dependencies()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "GO_VERSION", "envoy_dependencies")

envoy_dependencies()

load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(go_version = GO_VERSION)

new_local_repository(
    name = "python_linux",
    build_file_content = """
cc_library(
    name = "python36-lib",
    srcs = ["lib/python3.6/config-3.6m-x86_64-linux-gnu/libpython3.6.so"],
    hdrs = glob(["include/python3.6/*.h"]),
    includes = ["include/python3.6"],
    visibility = ["//visibility:public"]
)
    """,
    path = "/usr",
)

# For PIP support:
load("@io_bazel_rules_python//python:pip.bzl", "pip_import", "pip_repositories")

pip_repositories()

# This rule translates the specified requirements.txt into
# @my_deps//:requirements.bzl, which itself exposes a pip_install method.
pip_import(
    name = "python_pip_deps",
    requirements = "//:requirements.txt",
)

# Load the pip_install symbol for my_deps, and create the dependencies'
# repositories.
load("@python_pip_deps//:requirements.bzl", "pip_install")

pip_install()
