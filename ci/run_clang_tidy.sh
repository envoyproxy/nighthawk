#!/bin/bash

set -e

# Quick syntax check of .clang-tidy.
clang-tidy -dump-config > /dev/null 2> clang-tidy-config-errors.txt
if [[ -s clang-tidy-config-errors.txt ]]; then
  cat clang-tidy-config-errors.txt
  rm clang-tidy-config-errors.txt
  exit 1
fi
rm clang-tidy-config-errors.txt

echo "Generating compilation database..."

cp -f .bazelrc .bazelrc.bak

function cleanup() {
  cp -f .bazelrc.bak .bazelrc
  rm -f .bazelrc.bak
}
trap cleanup EXIT

# bazel build need to be run to setup virtual includes, generating files which are consumed
# by clang-tidy
tools/gen_compilation_database.py --run_bazel_build --include_headers

LLVM_PREFIX=$(llvm-config --prefix)
"${LLVM_PREFIX}/share/clang/run-clang-tidy.py" -extra-arg-before=-xc++ -quiet -j ${NUM_CPUS}
