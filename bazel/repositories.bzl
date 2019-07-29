load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

ENVOY_COMMIT = "245888b2c01561670927427883dc7b9786cae13c"
ENVOY_SHA = "30ed03d1d064496679c606ca542d49d4e79455947bbe20815021774e05ca0f1d"

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
        sha256 = "6928ba22634d9a5b2752227309c9097708e790db6a285fa5c3f40a219bf7ee98",
        strip_prefix = "HdrHistogram_c-0.9.8",
        url = "https://github.com/HdrHistogram/HdrHistogram_c/archive/0.9.8.tar.gz",
    )

    http_archive(
        name = "io_bazel_rules_python",
        sha256 = RULES_PYTHON_SHA,
        strip_prefix = "rules_python-%s" % RULES_PYTHON_COMMIT,
        url = "https://github.com/bazelbuild/rules_python/archive/%s.tar.gz" % RULES_PYTHON_COMMIT,
    )
