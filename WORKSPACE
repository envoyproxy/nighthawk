workspace(name = "nighthawk")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "envoy",
    sha256 = "9c19940b25d87ab122bf904b5b8534f42f5aea730d0b09e810605e0c388ddbc0",
    strip_prefix = "envoy-b8325ac4167284b4d566c2779c15ec3c8473dc66",
    url = "https://github.com/envoyproxy/envoy/archive/b8325ac4167284b4d566c2779c15ec3c8473dc66.tar.gz"
)

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")
envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")
load("@envoy//bazel:cc_configure.bzl", "cc_configure")
envoy_dependencies()

load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

cc_configure()

http_archive(
    name = "dep_hdrhistogram_c",
    build_file_content = """
cc_library(
    name = "hdrhistogram_c",
    srcs = [
        "src/hdr_encoding.c",
        "src/hdr_histogram.c",
        "src/hdr_histogram_log.c",
        "src/hdr_interval_recorder.c",
        "src/hdr_thread.c",
        "src/hdr_time.c",
        "src/hdr_writer_reader_phaser.c",
    ],
    hdrs = [
        "src/hdr_atomic.h",
        "src/hdr_encoding.h",
        "src/hdr_endian.h",
        "src/hdr_histogram.h",
        "src/hdr_histogram_log.h",
        "src/hdr_interval_recorder.h",
        "src/hdr_tests.h",
        "src/hdr_thread.h",
        "src/hdr_time.h",
        "src/hdr_writer_reader_phaser.h",
    ],
    copts = [
        "-std=gnu99",
        "-Wno-implicit-function-declaration",
        "-Wno-error",
    ],
    visibility = ["//visibility:public"],
)
""",
    sha256 = "6928ba22634d9a5b2752227309c9097708e790db6a285fa5c3f40a219bf7ee98",
    strip_prefix = "HdrHistogram_c-0.9.8",
    url = "https://github.com/HdrHistogram/HdrHistogram_c/archive/0.9.8.tar.gz",
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains()
