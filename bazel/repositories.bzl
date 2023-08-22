load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Pure refactor: 1a37802c9c8b2a163305774bbbf2ba21318531d2
# .bazelrc change: 748d4d56ad433caed921866d61c6fbb0ed156a6b 
# docker changes: dd1059d851bfe2aae2357cbf5f5574a98acb1af6
# .bazel version change: c33afb435881d177a207080b91588a7edcb66bf2
# Encap of response code: d3c384ae5d7f9ea4e72e711e419d218af3e26a17
ENVOY_COMMIT = "8004b60fc9902f65b58c4a9153eb065d1a4152dc"
ENVOY_SHA = "030d940a0eab63eacc21c9b0b1296ac8f2313a13fc01c54138be83443dbb2ccb"

HDR_HISTOGRAM_C_VERSION = "0.11.2"  # October 12th, 2020
HDR_HISTOGRAM_C_SHA = "637f28b5f64de2e268131e4e34e6eef0b91cf5ff99167db447d9b2825eae6bad"

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
        # // clang-format off
        url = "https://github.com/HdrHistogram/HdrHistogram_c/archive/%s.tar.gz" % HDR_HISTOGRAM_C_VERSION,
        # // clang-format on
    )

    # // GRPC has a dependency on gtest which needs to be bound: https://github.com/grpc/grpc/commit/decc199ca8472b3e55b9779aafc0c682514b70c7 but envoy binds to googletest instead which doesn't seem to work in this case. https://github.com/envoyproxy/envoy/pull/16687/files#R507
    native.bind(
        name = "gtest",
        actual = "@com_google_googletest//:gtest",
    )
