load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

package(default_visibility = ["//visibility:public"])

bool_flag(
    name = "run_stress_tests",
    build_setting_default = False,
)

config_setting(
    name = "stress_tests_enabled",
    flag_values = {
        ":run_stress_tests": "True",
    },
)

config_setting(
    name = "github_ci_build",
    define_values = {"build_type": "github_ci"},
)
