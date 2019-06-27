#!/bin/bash

set -e

export BUILDIFIER_BIN="/usr/local/bin/buildifier"

function do_build () {
    bazel build $BAZEL_BUILD_OPTIONS --verbose_failures=true //:nighthawk_client //:nighthawk_test_server
}

function do_test() {
    bazel test $BAZEL_BUILD_OPTIONS $BAZEL_TEST_OPTIONS \
    --test_output=all \
    //test:nighthawk_test //test/server:http_test_server_filter_integration_test \
    //integration:integration_test
}

function do_test_with_valgrind() {
    apt-get update && apt-get install valgrind && \
    bazel build $BAZEL_BUILD_OPTIONS -c dbg //test:nighthawk_test && \
    nighthawk/tools/valgrind-tests.sh
}

function do_clang_tidy() {
    ci/run_clang_tidy.sh
}

function do_coverage() {
    ci/run_coverage.sh
}

function setup_gcc_toolchain() {
    export CC=gcc
    export CXX=g++
    echo "$CC/$CXX toolchain configured"
}

function setup_clang_toolchain() {
    export PATH=/usr/lib/llvm-8/bin:$PATH
    export CC=clang
    export CXX=clang++
    export ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-8/bin/llvm-symbolizer
    echo "$CC/$CXX toolchain configured"
}

function run_bazel() {
    declare -r BAZEL_OUTPUT="${SRCDIR}"/bazel.output.txt
    bazel $* | tee "${BAZEL_OUTPUT}"
    declare BAZEL_STATUS="${PIPESTATUS[0]}"
    if [ "${BAZEL_STATUS}" != "0" ]
    then
        declare -r FAILED_TEST_LOGS="$(grep "  /build.*test.log" "${BAZEL_OUTPUT}" | sed -e 's/  \/build.*\/testlogs\/\(.*\)/\1/')"
        cd bazel-testlogs
        for f in ${FAILED_TEST_LOGS}
        do
            echo "Failed test log ${f}"
            cp --parents -f $f "${ENVOY_FAILED_TEST_LOGS}"
        done
        exit "${BAZEL_STATUS}"
    fi
}

function do_asan() {
    echo "bazel ASAN/UBSAN debug build with tests"
    echo "Building and testing envoy tests..."
    cd "${SRCDIR}"
    run_bazel test ${BAZEL_TEST_OPTIONS} -c dbg --config=clang-asan //test:nighthawk_test //test/server:http_test_server_filter_integration_test
}

function do_tsan() {
    echo "bazel TSAN debug build with tests"
    echo "Building and testing envoy tests..."
    cd "${SRCDIR}"
    run_bazel test ${BAZEL_TEST_OPTIONS} -c dbg --config=clang-tsan //test:nighthawk_test //test/server:http_test_server_filter_integration_test
}

function do_check_format() {
    echo "check_format..."
    cd "${SRCDIR}"
    ./tools/check_format.sh check
    ./tools/format_python_tools.sh check
}

function do_fix_format() {
    echo "fix_format..."
    cd "${SRCDIR}"
    ./tools/check_format.sh fix
    ./tools/format_python_tools.sh fix
}

[ -z "${NUM_CPUS}" ] && export NUM_CPUS=`grep -c ^processor /proc/cpuinfo`

if [ -n "$CIRCLECI" ]; then
    if [[ -f "${HOME:-/root}/.gitconfig" ]]; then
        mv "${HOME:-/root}/.gitconfig" "${HOME:-/root}/.gitconfig_save"
        echo 1
    fi

    NUM_CPUS=8
    if [ "$1" == "coverage" ]; then
        NUM_CPUS=6
    fi
fi

 if grep 'docker\|lxc' /proc/1/cgroup; then
    # Create a fake home. Python site libs tries to do getpwuid(3) if we don't and the CI
    # Docker image gets confused as it has no passwd entry when running non-root
    # unless we do this.
    FAKE_HOME=/tmp/fake_home
    mkdir -p "${FAKE_HOME}"
    export HOME="${FAKE_HOME}"
    export PYTHONUSERBASE="${FAKE_HOME}"

    export BUILD_DIR=/build
    if [[ ! -d "${BUILD_DIR}" ]]
    then
        echo "${BUILD_DIR} mount missing - did you forget -v <something>:${BUILD_DIR}? Creating."
        mkdir -p "${BUILD_DIR}"
    fi

    # Environment setup.
    export USER=bazel
    export TEST_TMPDIR=/build/tmp
    export BAZEL="bazel"
fi

export BAZEL_EXTRA_TEST_OPTIONS="--test_env=ENVOY_IP_TEST_VERSIONS=v4only ${BAZEL_EXTRA_TEST_OPTIONS}"
export BAZEL_BUILD_OPTIONS=" \
--verbose_failures ${BAZEL_OPTIONS} --action_env=HOME --action_env=PYTHONUSERBASE \
--jobs=${NUM_CPUS} --show_task_finish --experimental_generate_json_trace_profile ${BAZEL_BUILD_EXTRA_OPTIONS}"
export BAZEL_TEST_OPTIONS="${BAZEL_BUILD_OPTIONS} --test_env=HOME --test_env=PYTHONUSERBASE \
--test_env=UBSAN_OPTIONS=print_stacktrace=1 \
--cache_test_results=no --test_output=all ${BAZEL_EXTRA_TEST_OPTIONS}"
[[ -z "${SRCDIR}" ]] && SRCDIR="${PWD}"

setup_clang_toolchain

if [ "$1" == "coverage" ]; then
    setup_gcc_toolchain
fi

case "$1" in
    build)
        do_build
        exit 0
    ;;
    test)
        do_test
        exit 0
    ;;
    test_with_valgrind)
        do_test_with_valgrind
        exit 0
    ;;
    clang_tidy)
        do_clang_tidy
        exit 0
    ;;
    coverage)
        do_coverage
        exit 0
    ;;
    asan)
        do_asan
        exit 0
    ;;
    tsan)
        do_tsan
        exit 0
    ;;
    check_format)
        do_check_format
        exit 0
    ;;
    fix_format)
        do_fix_format
        exit 0
    ;;
    *)
        echo "must be one of [build,test,clang_tidy,test_with_valgrind,coverage,asan,tsan]"
        exit 1
    ;;
esac
