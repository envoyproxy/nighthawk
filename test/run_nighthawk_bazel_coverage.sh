#!/bin/bash

# derived from test/run_envoy_bazel_coverage.sh over the Envoy repo.

set -eo pipefail
set +x
set -u

SRCDIR="${SRCDIR:=${PWD}}"
ENVOY_COVERAGE_DIR="${ENVOY_COVERAGE_DIR:=}"
COVERAGE_THRESHOLD=${COVERAGE_THRESHOLD:=0}

echo "Starting run_nighthawk_bazel_coverage.sh..."
echo "    PWD=$(pwd)"
echo "    SRCDIR=${SRCDIR}"
echo "    COVERAGE_THRESHOLD=${COVERAGE_THRESHOLD}"

COVERAGE_DIR="${SRCDIR}"/generated/coverage
rm -rf "${COVERAGE_DIR}"
mkdir -p "${COVERAGE_DIR}"

# This is the target that will be run to generate coverage data. It can be overridden by consumer
# projects that want to run coverage on a different/combined target.
# Command-line arguments take precedence over ${COVERAGE_TARGET}.
if [[ $# -gt 0 ]]; then
  COVERAGE_TARGETS=$*
elif [[ -n "${COVERAGE_TARGET}" ]]; then
  COVERAGE_TARGETS=${COVERAGE_TARGET}
else
  COVERAGE_TARGETS=//test/...
fi


# The environment variable CI is used to determine if some expensive tests that
# cannot run locally should be executed.
# E.g. test_http_h1_mini_stress_test_open_loop.
BAZEL_BUILD_OPTIONS+=" --test_timeout=900 --config=test-coverage --test_tag_filters=-nocoverage --test_env=ENVOY_IP_TEST_VERSIONS=v4only --action_env=CI"
bazel coverage ${BAZEL_BUILD_OPTIONS} --cache_test_results=no --test_output=all -- ${COVERAGE_TARGETS}
COVERAGE_DATA="${COVERAGE_DIR}/coverage.dat"

cp bazel-out/_coverage/_coverage_report.dat "${COVERAGE_DATA}"

read -ra GENHTML_ARGS <<< "${GENHTML_ARGS:-}"
# TEMP WORKAROUND FOR MOBILE
CWDNAME="$(basename "${SRCDIR}")"
if [[ "$CWDNAME" == "mobile" ]]; then
    for arg in "${GENHTML_ARGS[@]}"; do
        if [[ "$arg" == --erase-functions=* ]]; then
            mobile_args_present=true
        fi
    done
    if [[ "$mobile_args_present" != "true" ]]; then
        GENHTML_ARGS+=(
            --erase-functions=__cxx_global_var_init
            --ignore-errors "category,corrupt,inconsistent")
    fi
fi
GENHTML_ARGS=(
    --prefix "${PWD}"
    --output "${COVERAGE_DIR}"
    "${GENHTML_ARGS[@]}"
    "${COVERAGE_DATA}")
COVERAGE_VALUE="$(genhtml "${GENHTML_ARGS[@]}" | tee /dev/stderr | grep lines... | cut -d ' ' -f 4)"

COVERAGE_VALUE=${COVERAGE_VALUE%?}

echo "Zipping coverage report to ${SRCDIR}/coverage_html.zip".
zip -r "${SRCDIR}/coverage_html.zip" "${COVERAGE_DIR}"

[[ -z "${ENVOY_COVERAGE_DIR}" ]] || rsync -av "${COVERAGE_DIR}"/ "${ENVOY_COVERAGE_DIR}"

if [ "$COVERAGE_THRESHOLD" != "0" ]
then
  COVERAGE_FAILED=$(echo "${COVERAGE_VALUE}<${COVERAGE_THRESHOLD}" | bc)
  if test ${COVERAGE_FAILED} -eq 1; then
      echo Code coverage ${COVERAGE_VALUE} is lower than limit of ${COVERAGE_THRESHOLD}
      exit 1
  else
      echo Code coverage ${COVERAGE_VALUE} is good and higher than limit of ${COVERAGE_THRESHOLD}
  fi
fi
echo "HTML coverage report is in ${COVERAGE_DIR}/index.html"
echo "If running in Azure Pipelines, the coverage report is published as a pipeline artifact."
