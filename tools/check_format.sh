#!/bin/bash

set -e

# Using a path like ./../nighthawk is a workaround that allows us to skip Envoy
# specific code checks performed by @envoy//tools/code_format:check_format.py.
# TODO(https://github.com/envoyproxy/nighthawk/issues/815): Replace this
# workaround with a permanent solution.
CURRENT_DIRECTORY=${PWD##*/}
FULL_CHECK="./../$CURRENT_DIRECTORY"

TO_CHECK="${2:-$FULL_CHECK}"
# TODO(https://github.com/envoyproxy/nighthawk/issues/165): fully excluding everything
# from the build fixer isn't ideal.
bazel run @envoy//tools/code_format:check_format.py -- \
  --skip_envoy_build_rule_check  --namespace_check Nighthawk \
  --build_fixer_check_excluded_paths=$TO_CHECK \
  --include_dir_order envoy,nighthawk,external/source/envoy,external,api,common,source,exe,server,client,distributor,sink,grpcpp,request_source,test_common,test,user_defined_output \
  $1 $TO_CHECK

# The include checker doesn't support per-file checking, so we only
# run it when a full check is requested.
if [ $FULL_CHECK == $TO_CHECK ]; then
  bazel run //tools:check_envoy_includes.py
fi
