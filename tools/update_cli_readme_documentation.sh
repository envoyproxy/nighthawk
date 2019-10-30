#! /bin/bash

# Updates Nighthakws documentation with generated version of TCLAP --help output.

set -e

bazel run $BAZEL_BUILD_OPTIONS //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_client --readme README.md "$@"
bazel run $BAZEL_BUILD_OPTIONS //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_service --readme README.md "$@"
bazel run $BAZEL_BUILD_OPTIONS //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_output_transform --readme README.md "$@"
bazel run $BAZEL_BUILD_OPTIONS //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_test_server --readme source/server/README.md "$@"

