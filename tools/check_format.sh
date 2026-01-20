#!/bin/bash

set -e

FULL_CHECK="$PWD"

TO_CHECK="${2:-$FULL_CHECK}"
# TODO(https://github.com/envoyproxy/nighthawk/issues/165): fully excluding everything
# from the build fixer isn\'t ideal.
bazel run --spawn_strategy=local --remote_executor= @envoy//tools/code_format:check_format -- \
  --config_path=${PWD}/tools/code_format/config.yaml \
  --skip_envoy_build_rule_check  --namespace_check Nighthawk \
  --build_fixer_check_excluded_paths=$TO_CHECK \
  $1 $TO_CHECK

# The include checker doesn't support per-file checking, so we only
# run it when a full check is requested.
if [ $FULL_CHECK == $TO_CHECK ]; then
  bazel run //tools:check_envoy_includes.py
fi
