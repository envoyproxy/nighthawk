#!/bin/bash

set -e

bazel run @envoy//tools:check_format.py -- \
--skip_envoy_build_rule_check  --namespace_check Nighthawk \
--include_dir_order envoy,nighthawk,external/source/envoy,external,api,common,source,exe,server,client,test_common,test \
$1 $PWD

bazel run //tools:check_envoy_includes.py
