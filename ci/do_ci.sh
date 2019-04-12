#!/bin/bash -e

function do_build () {
    bazel build $BAZEL_BUILD_OPTIONS --verbose_failures=true //:nighthawk_client
}

function do_test() {
    bazel test $BAZEL_BUILD_OPTIONS $BAZEL_TEST_OPTIONS --test_output=all \
    //test:nighthawk_test
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
    export PATH=/usr/lib/llvm-7/bin:$PATH
    export CC=clang
    export CXX=clang++
    export ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-7/bin/llvm-symbolizer
    echo "$CC/$CXX toolchain configured"
}

# TODO(oschaaf): This is quite the hack, we want to revisit this and back it out
# once we figure out why bazel coverage isn't working as expected without doing
# this. Once we do figure that out, this hack *could* be rewritten to assert that the linker
# that gets used is the actual one we requested.
# Overrides both ld and ld.gold with llvm's ld.lld.
function override_linker_with_lld() {
    WORK_DIR=`mktemp -d -p "$DIR"`
    export PATH="$WORK_DIR:$PATH"
    ln -s "$(which ld.lld)" "$WORK_DIR/ld"
    # We write a script to call ld.lld as 'ld' because it objects to being
    # called `ld.gold` on some systems.
    script="#!/bin/bash
$WORK_DIR/ld \$@
"
    echo "$script" > "$WORK_DIR/ld.gold"
    chmod +x "$WORK_DIR/ld.gold"
    echo "lld linker override in place:"
    # As a test, we output the version of ld.lld, which has some diagnostic value as well.
    $WORK_DIR/ld.gold -v
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
    override_linker_with_lld
    cd "${SRCDIR}"
    run_bazel test ${BAZEL_TEST_OPTIONS} -c dbg --config=clang-asan //test:nighthawk_test
}

function do_tsan() {
    echo "bazel TSAN debug build with tests"
    echo "Building and testing envoy tests..."
    override_linker_with_lld
    cd "${SRCDIR}"
    run_bazel test ${BAZEL_TEST_OPTIONS} -c dbg --config=clang-tsan //test:nighthawk_test
}

[ -z "${NUM_CPUS}" ] && NUM_CPUS=`grep -c ^processor /proc/cpuinfo`

if [ -n "$CIRCLECI" ]; then
    # TODO(oschaaf): hack, this should be done in .circleci/config.yml
    git submodule update --init --recursive
    if [[ -f "${HOME:-/root}/.gitconfig" ]]; then
        mv "${HOME:-/root}/.gitconfig" "${HOME:-/root}/.gitconfig_save"
        echo 1
    fi

    NUM_CPUS=8
    if [ "$1" == "coverage" ]; then
        NUM_CPUS=6
    fi
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
    ;;
    test)
        do_test
    ;;
    test_with_valgrind)
        do_test_with_valgrind
    ;;
    clang_tidy)
        export RUN_FULL_CLANG_TIDY=1
        do_clang_tidy
    ;;
    coverage)
        do_coverage
    ;;
    asan)
        do_asan
    ;;
    tsan)
        do_tsan
    ;;
    *)
        echo "must be one of [build,test,clang_tidy,test_with_valgrind,coverage,asan,tsan]"
        exit 1
    ;;
esac