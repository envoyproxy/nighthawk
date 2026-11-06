licenses(["notice"])  # Apache 2

# Hermetic GCC toolchain for Nighthawk using toolshed
toolchain(
    name = "linux_x86_64_toolchain",
    exec_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    target_settings = ["@envoy//bazel:gcc_requested"],
    toolchain = "@gcc_linux_x86_64//:cc_toolchain",
    toolchain_type = "@rules_cc//cc:toolchain_type",
)
