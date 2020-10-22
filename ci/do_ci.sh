#!/bin/bash

set -eo pipefail
set +x
set -u

if [ $# -eq 0 ]; then
    set -- "help"
fi

export BUILDIFIER_BIN="${BUILDIFIER_BIN:=/usr/local/bin/buildifier}"
export BUILDOZER_BIN="${BUILDOZER_BIN:=/usr/local/bin/buildozer}"
export NUM_CPUS=${NUM_CPUS:=$(grep -c ^processor /proc/cpuinfo)}
export CIRCLECI=${CIRCLECI:=""}
export BAZEL_EXTRA_TEST_OPTIONS=${BAZEL_EXTRA_TEST_OPTIONS:=""}
export BAZEL_OPTIONS=${BAZEL_OPTIONS:=""}
export BAZEL_BUILD_EXTRA_OPTIONS=${BAZEL_BUILD_EXTRA_OPTIONS:=""}
export SRCDIR=${SRCDIR:="${PWD}"}
export CLANG_FORMAT=clang-format

function do_build () {
    bazel build $BAZEL_BUILD_OPTIONS //:nighthawk
    tools/update_cli_readme_documentation.sh --mode check
}

function do_opt_build () {
    bazel build $BAZEL_BUILD_OPTIONS -c opt --define tcmalloc=gperftools //:nighthawk
    bazel build $BAZEL_BUILD_OPTIONS -c opt --define tcmalloc=gperftools //benchmarks:benchmarks
}

function do_test() {
    bazel build -c dbg $BAZEL_BUILD_OPTIONS //test/...
    bazel test -c dbg $BAZEL_TEST_OPTIONS --test_output=all //test/...
}

function do_clang_tidy() {
    ci/run_clang_tidy.sh
}

function do_unit_test_coverage() {
    export TEST_TARGETS="//test/... -//test:python_test"
    export COVERAGE_THRESHOLD=94.0
    echo "bazel coverage build with tests ${TEST_TARGETS}"
    test/run_nighthawk_bazel_coverage.sh ${TEST_TARGETS}
    exit 0
}

function do_integration_test_coverage() {
    export TEST_TARGETS="//test:python_test"
    export COVERAGE_THRESHOLD=78.6
    echo "bazel coverage build with tests ${TEST_TARGETS}"
    test/run_nighthawk_bazel_coverage.sh ${TEST_TARGETS}
    exit 0
}

function setup_gcc_toolchain() {
    export CC=gcc
    export CXX=g++
    export BAZEL_COMPILER=gcc
    echo "$CC/$CXX toolchain configured"
}

function setup_clang_toolchain() {
    export PATH=/opt/llvm/bin:$PATH
    export CC=clang
    export CXX=clang++
    export ASAN_SYMBOLIZER_PATH=/opt/llvm/bin/llvm-symbolizer
    export BAZEL_COMPILER=clang
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

function do_sanitizer() {
    CONFIG="$1"
    echo "bazel $CONFIG debug build with tests"
    echo "Building and testing Nighthawk tests..."
    cd "${SRCDIR}"

    # We build this in steps to avoid running out of memory in CI
    run_bazel build ${BAZEL_TEST_OPTIONS} -c dbg --config="$CONFIG" -- //source/exe/... && \
    run_bazel build ${BAZEL_TEST_OPTIONS} -c dbg --config="$CONFIG" -- //source/server/... && \
    run_bazel build ${BAZEL_TEST_OPTIONS} -c dbg --config="$CONFIG" -- //test/mocks/... && \
    run_bazel build ${BAZEL_TEST_OPTIONS} -c dbg --config="$CONFIG" -- //test/... && \
    run_bazel test ${BAZEL_TEST_OPTIONS} -c dbg --config="$CONFIG" -- //test/...
}

function cleanup_benchmark_artifacts {
    # TODO(oschaaf): we clean the tmp dir above from uninteresting stuff
    # that crept into the tmp/output directory. The cruft gets in there because
    # other tooling also responds to the TMPDIR environment variable, which in retrospect
    # was a bad choice.
    # Consider using a different environment variable for the benchmark tooling
    # to use for this.
    size=${#TMPDIR}
    if [ $size -gt 4 ] && [ -d "${TMPDIR}" ]; then
        rm -rf ${TMPDIR}/tmp.*
    fi
}

function do_benchmark_with_own_binaries() {
    echo "Running benchmark framework with own binaries"
    cd "${SRCDIR}"
    # Benchmark artifacts will be dropped into this directory:
    export TMPDIR="${SRCDIR}/generated"
    mkdir -p "${TMPDIR}"
    trap cleanup_benchmark_artifacts EXIT
    run_bazel test ${BAZEL_TEST_OPTIONS} --test_summary=detailed \
        --test_arg=--log-cli-level=info \
        --test_env=HEAPPROFILE= \
        --test_env=HEAPCHECK= \
        --compilation_mode=opt \
        --cxxopt=-g \
        --cxxopt=-ggdb3 \
        --define tcmalloc=gperftools \
        //benchmarks:*
}

function do_check_format() {
    echo "check_format..."
    cd "${SRCDIR}"
    ./tools/check_format.sh check
    ./tools/format_python_tools.sh check
}

function do_docker() {
    echo "docker..."
    cd "${SRCDIR}"
    # Note that we implicly test the opt build in CI here
    do_opt_build
    ./ci/docker/docker_build.sh
    ./ci/docker/docker_push.sh
    ./ci/docker/benchmark_build.sh
}

function do_fix_format() {
    echo "fix_format..."
    cd "${SRCDIR}"
    ./tools/check_format.sh fix
    ./tools/format_python_tools.sh fix
}

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
--experimental_local_memory_estimate \
--show_task_finish --experimental_generate_json_trace_profile ${BAZEL_BUILD_EXTRA_OPTIONS}"

if [ -n "$CIRCLECI" ]; then
    if [[ -f "${HOME:-/root}/.gitconfig" ]]; then
        mv "${HOME:-/root}/.gitconfig" "${HOME:-/root}/.gitconfig_save"
        echo 1
    fi
    NUM_CPUS=8
    if [[ "$1" == "test_gcc" ]]; then
        NUM_CPUS=4
    fi
    echo "Running with ${NUM_CPUS} cpus"
    BAZEL_BUILD_OPTIONS="${BAZEL_BUILD_OPTIONS} --jobs=${NUM_CPUS}"
fi

export BAZEL_TEST_OPTIONS="${BAZEL_BUILD_OPTIONS} --test_env=HOME --test_env=PYTHONUSERBASE \
--test_env=UBSAN_OPTIONS=print_stacktrace=1 \
--cache_test_results=no --test_output=all ${BAZEL_EXTRA_TEST_OPTIONS}"

case "$1" in
    build)
        setup_clang_toolchain
        do_build
        exit 0
    ;;
    test)
        setup_clang_toolchain
        do_test
        exit 0
    ;;
    test_gcc)
        setup_gcc_toolchain
        do_test
        exit 0
    ;;
    clang_tidy)
        setup_clang_toolchain
        RUN_FULL_CLANG_TIDY=1 do_clang_tidy
        exit 0
    ;;
    coverage)
        setup_clang_toolchain
        do_unit_test_coverage
        exit 0
    ;;
    coverage_integration)
        setup_clang_toolchain
        do_integration_test_coverage
        exit 0
    ;;
    asan)
        setup_clang_toolchain
        do_sanitizer "clang-asan"
        exit 0
    ;;
    tsan)
        setup_clang_toolchain
        do_sanitizer "clang-tsan"
        exit 0
    ;;
    docker)
        setup_clang_toolchain
        do_docker
        exit 0
    ;;
    check_format)
        setup_clang_toolchain
        do_check_format
        exit 0
    ;;
    fix_format)
        setup_clang_toolchain
        do_fix_format
        exit 0
    ;;
    benchmark_with_own_binaries)
        setup_clang_toolchain
        do_benchmark_with_own_binaries
        exit 0
    ;;
    opt_build)
        setup_clang_toolchain
        do_opt_build
        exit 0
    ;;
    *)
        echo "must be one of [opt_build, build,test,clang_tidy,coverage,coverage_integration,asan,tsan,benchmark_with_own_binaries,docker,check_format,fix_format,test_gcc]"
        exit 1
    ;;
esac
