load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

ENVOY_COMMIT = "2a4f7dae855ef81376a8aee6201ea19ea36ef8e2"  # August 12th, 2020
ENVOY_SHA = "88d0eaca79178b6ab1c1443dfc23fb5cad0c9559ad3272a3114f3fb1bd905592"

HDR_HISTOGRAM_C_VERSION = "0.11.0"  # July 14th, 2020
HDR_HISTOGRAM_C_SHA = "c00696b3d81776675aa2bc62d3642e31bd8a48cc9619c9bd7d4a78762896e353"

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
        sha256 = HDR_HISTOGRAM_C_SHA,
        strip_prefix = "HdrHistogram_c-%s" % HDR_HISTOGRAM_C_VERSION,
        url = "https://github.com/HdrHistogram/HdrHistogram_c/archive/%s.tar.gz" % HDR_HISTOGRAM_C_VERSION,
    )
