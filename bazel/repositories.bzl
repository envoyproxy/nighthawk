load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

ENVOY_COMMIT = "8db16823ec827dcc9a7c8694934f4e3ddcd43a6d"  # December 3rd, 2019
ENVOY_SHA = ""

RULES_PYTHON_COMMIT = "fdbb17a4118a1728d19e638a5291b4c4266ea5b8"
RULES_PYTHON_SHA = "e6f57cac5d979a399113347832732767b7c1f35cfb8e73bc06e3a4f7c1e96f44"

def nighthawk_dependencies():
    http_archive(
        name = "envoy",
        sha256 = ENVOY_SHA,
        strip_prefix = "envoy-%s" % ENVOY_COMMIT,
        url = "https://github.com/envoyproxy/envoy/archive/%s.tar.gz" % ENVOY_COMMIT,
    )
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
    deps = ["//external:zlib"],
    visibility = ["//visibility:public"],
)
  """,
        sha256 = "43859763552a5cda0d3c8b0d81b2ae15d3e341df73b6c959095434fd0e2239e7",
        strip_prefix = "HdrHistogram_c-0.9.11",
        url = "https://github.com/HdrHistogram/HdrHistogram_c/archive/0.9.11.tar.gz",
    )

    http_archive(
        name = "io_bazel_rules_python",
        sha256 = RULES_PYTHON_SHA,
        strip_prefix = "rules_python-%s" % RULES_PYTHON_COMMIT,
        url = "https://github.com/bazelbuild/rules_python/archive/%s.tar.gz" % RULES_PYTHON_COMMIT,
    )
