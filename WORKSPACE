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
