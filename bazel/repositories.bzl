load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

ENVOY_COMMIT = "24f44fc0e781cbcd6251f5286bfe4f16600c167c"  # November 5th, 2019
ENVOY_SHA = "791fe437767f0d58572adf57ee2ad4417ae62baef05ff7489cfb697491a2af76"

RULES_PYTHON_COMMIT = "fdbb17a4118a1728d19e638a5291b4c4266ea5b8"
RULES_PYTHON_SHA = "9a3d71e348da504a9c4c5e8abd4cb822f7afb32c613dc6ee8b8535333a81a938"

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
