#!/bin/bash

set -e

echo "Generating compilation database..."
# The compilation database generate script doesn't support passing build options via CLI.
# Writing them into bazelrc
echo "build ${BAZEL_BUILD_OPTIONS}" >> .bazelrc

# bazel build need to be run to setup virtual includes, generating files which are consumed
# by clang-tidy
tools/gen_compilation_database.py --run_bazel_build --include_headers
set -x
run-clang-tidy-8 -extra-arg-before=-xc++ -quiet -j "${NUM_CPUS}"
