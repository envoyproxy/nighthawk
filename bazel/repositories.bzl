load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

ENVOY_COMMIT = "582ab4a353ac2c19f4326115b471cebeabe7ae8a"  # April 17th, 2020
ENVOY_SHA = "fd019d30f45bbc781e5e95324e3b16abcd23beca19baf0aa762bd39c602ddac1"

RULES_PYTHON_COMMIT = "dd7f9c5f01bafbfea08c44092b6b0c8fc8fcb77f"  # Feb 22nd, 2020
RULES_PYTHON_SHA = "0aa9ec790a58053e3ab5af397879b267a625955f8297c239b2d8559c6773397b"

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

    http_archive(
        name = "io_bazel_rules_python",
        sha256 = RULES_PYTHON_SHA,
        strip_prefix = "rules_python-%s" % RULES_PYTHON_COMMIT,
        url = "https://github.com/bazelbuild/rules_python/archive/%s.tar.gz" % RULES_PYTHON_COMMIT,
    )
