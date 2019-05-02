#!/bin/bash

bazel run @envoy//tools:check_format.py -- \
  --skip_envoy_build_rule_check  --namespace_check Nighthawk --include_dir_order envoy \
  --include_dir_order envoy,nighthawk,common,source,exe,server,client,extensions,test \
  $1 $PWD
