#!/bin/bash

set -e

[[ -z "${SRCDIR}" ]] && SRCDIR="${PWD}"
[[ -z "${BAZEL_COVERAGE}" ]] && BAZEL_COVERAGE=bazel
[[ -z "${VALIDATE_COVERAGE}" ]] && VALIDATE_COVERAGE=true

# This is the target that will be run to generate coverage data. It can be overridden by consumer
# projects that want to run coverage on a different/combined target.
[[ -z "${COVERAGE_TARGET}" ]] && COVERAGE_TARGET="//nighthawk/test/..."

bazel clean

# Generate coverage data.
"${BAZEL_COVERAGE}" coverage ${BAZEL_TEST_OPTIONS} \
"${COVERAGE_TARGET}"  \
--experimental_cc_coverage \
--instrumentation_filter=//nighthawk/source/...,//nighthawk/include/... \
--coverage_report_generator=@bazel_tools//tools/test/CoverageOutputGenerator/java/com/google/devtools/coverageoutputgenerator:Main \
--combined_report=lcov

# Generate HTML
declare -r COVERAGE_DIR="${SRCDIR}"/generated/coverage
declare -r COVERAGE_SUMMARY="${COVERAGE_DIR}/coverage_summary.txt"
mkdir -p "${COVERAGE_DIR}"
genhtml bazel-out/_coverage/_coverage_report.dat --output-directory="${COVERAGE_DIR}" | tee "${COVERAGE_SUMMARY}"

[[ -z "${ENVOY_COVERAGE_DIR}" ]] || rsync -av "${COVERAGE_DIR}"/ "${ENVOY_COVERAGE_DIR}"

if [ "$VALIDATE_COVERAGE" == "true" ]
then
    COVERAGE_VALUE=$(grep -Po '.*lines[.]*: \K(\d|\.)*' "${COVERAGE_SUMMARY}")
    # TODO(oschaaf): The target is 97.5%, so up this whenever possible in follow ups.
    COVERAGE_THRESHOLD=96.6
    COVERAGE_FAILED=$(echo "${COVERAGE_VALUE}<${COVERAGE_THRESHOLD}" | bc)
    
    echo "HTML coverage report is in ${COVERAGE_DIR}/coverage.html"
    
    if test ${COVERAGE_FAILED} -eq 1; then
        echo Code coverage ${COVERAGE_VALUE} is lower than limit of ${COVERAGE_THRESHOLD}
        exit 1
    else
        echo Code coverage ${COVERAGE_VALUE} is good and higher than limit of ${COVERAGE_THRESHOLD}
    fi
fi
