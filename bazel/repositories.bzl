load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

ENVOY_COMMIT = "84affaad1173a1e48bbee5b3049c305a555eb3b2"  # July 13th, 2020
ENVOY_SHA = "df6c0853bbeaf3e36a4db9a5ccbf74a9cff9e436ed52a39aa0d683700a90e4f5"

HDR_HISTOGRAM_C_VERSION = "0.9.13"  # Feb 22nd, 2020
HDR_HISTOGRAM_C_SHA = "2bd4a4631b64f2f8cf968ef49dd03ff3c51b487c3c98a01217ae4cf4a35b8310"

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
