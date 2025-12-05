load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

ENVOY_COMMIT = "e593c7820b15aab315ddc2b615bd639012288469"
ENVOY_SHA = "6f5614477880fc392844e2b63efb9b55774e908e1bc5f18e6ee57068f76addf8"

HDR_HISTOGRAM_C_VERSION = "0.11.8"  # June 18th, 2025
HDR_HISTOGRAM_C_SHA = "bb95351a6a8b242dc9be1f28562761a84d4cf0a874ffc90a9b630770a6468e94"

def nighthawk_dependencies():
    http_archive(
        name = "envoy",
        sha256 = ENVOY_SHA,
        strip_prefix = "envoy-%s" % ENVOY_COMMIT,
        # // clang-format off: Envoy's format check: Only repository_locations.bzl may contains URL references
        url = "https://github.com/envoyproxy/envoy/archive/%s.tar.gz" % ENVOY_COMMIT,
        # // clang-format on
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
        "src/hdr_malloc.h",
        "src/hdr_thread.c",
        "src/hdr_time.c",
        "src/hdr_writer_reader_phaser.c",
    ],
    hdrs = [
        "src/hdr_atomic.h",
        "src/hdr_encoding.h",
        "src/hdr_endian.h",
        "include/hdr/hdr_histogram.h",
        "include/hdr/hdr_histogram_log.h",
        "include/hdr/hdr_interval_recorder.h",
        "include/hdr/hdr_writer_reader_phaser.h",
        "src/hdr_tests.h",
        "include/hdr/hdr_thread.h",
        "include/hdr/hdr_time.h",
    ],
    includes = ["include"],
    copts = [
        "-std=gnu99",
        "-Wno-implicit-function-declaration",
        "-Wno-error",
    ],
    deps = ["@zlib",],
    visibility = ["//visibility:public"],
)
  """,
        sha256 = HDR_HISTOGRAM_C_SHA,
        strip_prefix = "HdrHistogram_c-%s" % HDR_HISTOGRAM_C_VERSION,
        # // clang-format off
        url = "https://github.com/HdrHistogram/HdrHistogram_c/archive/%s.tar.gz" % HDR_HISTOGRAM_C_VERSION,
        # // clang-format on
    )

    # // GRPC has a dependency on gtest which needs to be bound: https://github.com/grpc/grpc/commit/decc199ca8472b3e55b9779aafc0c682514b70c7 but envoy binds to googletest instead which doesn't seem to work in this case. https://github.com/envoyproxy/envoy/pull/16687/files#R507
    native.bind(
        name = "gtest",
        actual = "@com_google_googletest//:gtest",
    )
