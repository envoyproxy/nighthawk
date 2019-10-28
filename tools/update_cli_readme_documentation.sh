#! /bin/bash

set -e

bazel run -c dbg //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_client --readme README.md && \
bazel run -c dbg //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_service --readme README.md && \
bazel run -c dbg //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_output_transform --readme README.md && \
bazel run -c dbg //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_test_server --readme source/server/README.md && \

exit 0