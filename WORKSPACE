workspace(name = "nighthawk")

load("//bazel:repositories.bzl", "nighthawk_dependencies")

nighthawk_dependencies()

local_repository(
    name = "envoy_build_config",
    path = ".",
)

load("@envoy//bazel:api_binding.bzl", "envoy_api_binding")

envoy_api_binding()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repo.bzl", "envoy_repo")

envoy_repo()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("@envoy//bazel:repositories_extra.bzl", "envoy_dependencies_extra")

envoy_dependencies_extra()

load("@envoy//bazel:python_dependencies.bzl", "envoy_python_dependencies")

envoy_python_dependencies()

load("@envoy//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()

load("//bazel:python_dependencies.bzl", "nighthawk_python_dependencies")

nighthawk_python_dependencies()

load("@nh_pip3//:requirements.bzl", nh_pip3 = "install_deps")

nh_pip3()
