#! /bin/bash

# Updates Nighthakws documentation with generated version of TCLAP --help output.

set -e

bazel run $BAZEL_BUILD_OPTIONS //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_client --readme README.md "$@"
bazel run $BAZEL_BUILD_OPTIONS //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_service --readme README.md "$@"
bazel run $BAZEL_BUILD_OPTIONS //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_output_transform --readme README.md "$@"
bazel run $BAZEL_BUILD_OPTIONS //tools:update_cli_readme_documentation -- --binary bazel-bin/nighthawk_test_server --readme source/server/README.md "$@"

if [ "$2" == "fix" ]; then
    # When we're fixing the CLI docs, we run a fix format afterwards:
    # The generated CLI help ends up with "over-enthusiastic spaces", which we will fix here.
    tools/check_format.sh fix
fi

echo "Docs are up to date"