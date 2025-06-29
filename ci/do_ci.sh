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
export BAZEL_EXTRA_TEST_OPTIONS=${BAZEL_EXTRA_TEST_OPTIONS:=""}
export BAZEL_OPTIONS=${BAZEL_OPTIONS:=""}
export BAZEL_BUILD_EXTRA_OPTIONS=${BAZEL_BUILD_EXTRA_OPTIONS:=""}
export SRCDIR=${SRCDIR:="${PWD}"}
export CLANG_FORMAT=clang-format
export NIGHTHAWK_BUILD_ARCH=$(uname -m)
export BAZEL_REMOTE_CACHE=${BAZEL_REMOTE_CACHE:=""}
# The directory to copy built binaries to.
export BUILD_DIR=""

# We build in steps to avoid running out of memory in CI.
# This list doesn't have to be complete, execution of bazel test will build any
# remaining targets.
# The order matters here, dependencies are placed before dependents.
BUILD_PARTS=(
    "//api/..."
    "//source/exe/..."
    "//source/server/..."
    "//source/request_source/..."
    "//source/adaptive_load/..."
    "//test/mocks/..."
    "//test/..."
)

#######################################
# Runs the specified command on all the BUILD_PARTS.
# Arguments:
#   The command to execute, each part will be appended as the last argument to
#   this command.
# Returns:
#   0 on success, exits with return code 1 on failure.
#######################################
function run_on_build_parts() {
    local command="$1"
    for part in ${BUILD_PARTS[@]}; do
        echo "run_on_build_parts: running command $command $part"
        eval "$command $part"
        if (( $? != 0 )); then
            echo "Error executing $command $part."
            exit 1
        fi
    done
}

#######################################
# Conditionally copies binaries from bazel-bin into the specified directory.
# Arguments:
#   An optional directory to copy the built binaries into. If not provided,
#   binaries won't be copied anywhere and this function is a no-op.
# Returns:
#   0 on success, exits with return code 1 on failure.
#######################################
function maybe_copy_binaries_to_directory () {
    if [ -n "${BUILD_DIR}" ]; then
      echo "Copying built binaries to ${BUILD_DIR}."
      for BINARY_NAME in \
        "nighthawk_adaptive_load_client" \
        "nighthawk_client" \
        "nighthawk_output_transform" \
        "nighthawk_service" \
        "nighthawk_test_server"; do
        cp -vf bazel-bin/${BINARY_NAME} ${BUILD_DIR}
      done
    fi
}

#######################################
# Builds non-optimized (fastbuild) binaries of Nighthawk.
#
# Verifies documentation for the binaries is updated.
#
# Arguments:
#   An optional directory to copy the built binaries into. If not provided,
#   binaries won't be copied anywhere.
# Returns:
#   0 on success, exits with return code 1 on failure.
#######################################
function do_build () {
    bazel build $BAZEL_BUILD_OPTIONS //:nighthawk
    tools/update_cli_readme_documentation.sh --mode check
    maybe_copy_binaries_to_directory
}


#######################################
# Builds optimized binaries of Nighthawk.
# Arguments:
#   An optional directory to copy the built binaries into. If not provided,
#   binaries won't be copied anywhere.
# Returns:
#   0 on success, exits with return code 1 on failure.
#######################################
function do_opt_build () {
    bazel build \
          --remote_download_toplevel \
          $BAZEL_BUILD_OPTIONS \
          -c opt \
          --define tcmalloc=gperftools \
          //:nighthawk
    bazel build \
          --remote_download_toplevel \
          $BAZEL_BUILD_OPTIONS \
          -c opt \
          --define tcmalloc=gperftools \
          //benchmarks:benchmarks
    maybe_copy_binaries_to_directory
}

function do_test() {
    # Determine if we should run stress tests based on the branch
    # E.g. test_http_h1_mini_stress_test_open_loop.
    if [[ -n "${GH_BRANCH:-}" ]]; then
        STRESS_TEST_FLAG="--//test/config:run_stress_tests=True"
        BUILD_TYPE_FLAG="--define build_type=github_ci"
    else
        STRESS_TEST_FLAG="--//test/config:run_stress_tests=False"
        BUILD_TYPE_FLAG=""
    fi
    run_on_build_parts "bazel build -c dbg $BAZEL_BUILD_OPTIONS $STRESS_TEST_FLAG $BUILD_TYPE_FLAG"
    bazel test -c dbg $BAZEL_TEST_OPTIONS $STRESS_TEST_FLAG $BUILD_TYPE_FLAG //test/...
}

function do_clang_tidy() {
    # clang-tidy will warn on standard library issues with libc++
    BAZEL_BUILD_OPTIONS=("--config=clang" "${BAZEL_BUILD_OPTIONS[@]}")
    BAZEL_BUILD_OPTIONS="${BAZEL_BUILD_OPTIONS[*]}" ci/run_clang_tidy.sh
}

function do_unit_test_coverage() {
    export TEST_TARGETS="//test/... -//test:python_test"
    # TODO(https://github.com/envoyproxy/nighthawk/issues/747): Increase back to 93.2 when coverage flakiness address
    ENVOY_GENHTML_ARGS=(
            --ignore-errors "category,corrupt,inconsistent")
        GENHTML_ARGS="${ENVOY_GENHTML_ARGS[*]}"
    export GENHTML_ARGS
    export COVERAGE_THRESHOLD=91.5
    echo "bazel coverage build with tests ${TEST_TARGETS}"
    test/run_nighthawk_bazel_coverage.sh ${TEST_TARGETS}
    exit 0
}

function do_integration_test_coverage() {
    export TEST_TARGETS="//test:python_test"
    ENVOY_GENHTML_ARGS=(
            --ignore-errors "category,corrupt,inconsistent")
        GENHTML_ARGS="${ENVOY_GENHTML_ARGS[*]}"
    export GENHTML_ARGS
    # TODO(#830): Raise the integration test coverage.
    # TODO(dubious90): Raise this back up to at least 73.
    export COVERAGE_THRESHOLD=72.9
    echo "bazel coverage build with tests ${TEST_TARGETS}"
    test/run_nighthawk_bazel_coverage.sh ${TEST_TARGETS}
    exit 0
}

function setup_gcc_toolchain() {
    export CC=gcc
    export CXX=g++
    export BAZEL_COMPILER=gcc
    BAZEL_BUILD_OPTIONS="$BAZEL_BUILD_OPTIONS --config=gcc"
    BAZEL_TEST_OPTIONS="$BAZEL_TEST_OPTIONS --config=gcc"
    [[ "${NIGHTHAWK_BUILD_ARCH}" == "aarch64" ]] && BAZEL_BUILD_OPTIONS="$BAZEL_BUILD_OPTIONS --copt -march=armv8-a+crypto"
    [[ "${NIGHTHAWK_BUILD_ARCH}" == "aarch64" ]] && BAZEL_TEST_OPTIONS="$BAZEL_TEST_OPTIONS --copt -march=armv8-a+crypto"
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
    # The Envoy build system now uses hermetic SAN libraries that come with
    # Bazel. Those are built with libc++ instead of the GCC libstdc++.
    # Explicitly setting --config=libc++ to avoid duplicate symbols.
    run_on_build_parts "run_bazel build ${BAZEL_TEST_OPTIONS} -c dbg --config=$CONFIG --config=libc++ --"
    run_bazel test ${BAZEL_TEST_OPTIONS} -c dbg --config="$CONFIG" --config=libc++ -- //test/...
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
    # Note that we implicitly test the opt build in CI here.
    echo "do_docker: Running do_opt_build."
    do_opt_build
    echo "do_docker: Running ci/docker/docker_build.sh."
    ./ci/docker/docker_build.sh
    echo "do_docker: Running ci/docker/docker_push.sh."
    ./ci/docker/docker_push.sh
    echo "do_docker: Running ci/docker/benchmark_build.sh."
    ./ci/docker/benchmark_build.sh
    echo "do_docker: Running ci/docker/benchmark_push.sh."
    ./ci/docker/benchmark_push.sh
}

function do_fix_format() {
    echo "fix_format..."
    cd "${SRCDIR}"
    ./tools/check_format.sh fix
    ./tools/format_python_tools.sh fix
}

if grep 'docker\|lxc' /proc/1/cgroup; then
    export BUILD_DIR=/build
    echo "Running in Docker, built binaries will be copied into ${BUILD_DIR}."
    if [[ ! -d "${BUILD_DIR}" ]]
    then
        echo "${BUILD_DIR} mount missing - did you forget -v <something>:${BUILD_DIR}? Creating."
        mkdir -p "${BUILD_DIR}"
    fi

    # Environment setup.
    export BAZEL="bazel"
fi

export BAZEL_EXTRA_TEST_OPTIONS="--test_env=ENVOY_IP_TEST_VERSIONS=v4only ${BAZEL_EXTRA_TEST_OPTIONS}"
export BAZEL_BUILD_OPTIONS=" \
--verbose_failures ${BAZEL_OPTIONS} \
--experimental_generate_json_trace_profile ${BAZEL_BUILD_EXTRA_OPTIONS}"

echo "Running with ${NUM_CPUS} cpus and BAZEL_BUILD_OPTIONS: ${BAZEL_BUILD_OPTIONS}"

export BAZEL_TEST_OPTIONS="${BAZEL_BUILD_OPTIONS} \
--test_env=UBSAN_OPTIONS=print_stacktrace=1 \
--cache_test_results=no --test_output=errors ${BAZEL_EXTRA_TEST_OPTIONS}"

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
        do_sanitizer "asan"
        exit 0
    ;;
    tsan)
        setup_clang_toolchain
        do_sanitizer "tsan"
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
        echo "must be one of [opt_build,build,test,clang_tidy,coverage,coverage_integration,asan,tsan,benchmark_with_own_binaries,docker,check_format,fix_format,test_gcc]"
        exit 1
    ;;
esac
