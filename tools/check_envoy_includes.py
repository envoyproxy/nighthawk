#!/usr/bin/env python3
"""Ensure that includes are referenced via external/envoy."""

import os
import re
import sys
import subprocess
from pathlib import Path


def _get_inspection_targets_from_dir(dir):
  result = []
  result.extend(Path(dir).rglob("*.cc"))
  result.extend(Path(dir).rglob("*.h"))
  return result


def _inspect_line(bazel_output_base, file_path, line):
  match = re.findall(r'#include "([^"]*)"', line)
  if len(match) == 1:
    path = match[0]
    if path.startswith("external/") or path.startswith("envoy/"):
      return True
    found_in_nighthawk_sources = os.path.isfile(path) or os.path.isfile(
        "source/" + path) or os.path.isfile("include/" + path)
    if not found_in_nighthawk_sources:
      alternative_found = False
      potential_envoy_path = os.path.join(bazel_output_base, "external/envoy/", path)
      alternative_found = os.path.isfile(potential_envoy_path)

      if not alternative_found:
        potential_envoy_path = os.path.join(bazel_output_base, "external/envoy/source", path)
        alternative_found = os.path.isfile(potential_envoy_path)

      if alternative_found:
        sys.stderr.writelines("Bad include in file " + str(file_path) + ": " + path)
        sys.stderr.writelines(" (Possible fixed path: %s" %
                              potential_envoy_path[len(bazel_output_base) + 1:] + ")\n")
        return False

  return True


def _inspect_file(bazel_output_base, file_path):
  offending_lines = []
  with open(str(file_path), encoding='utf-8') as f:
    lines = f.readlines()
    offending_lines.extend(
        line for line in lines if not _inspect_line(bazel_output_base, file_path, line.strip()))
  return offending_lines


if __name__ == '__main__':
  bazel_output_base = subprocess.run(['bazel', 'info', 'output_base'],
                                     stdout=subprocess.PIPE).stdout.decode('utf-8').strip()
  workspace_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
  targets = _get_inspection_targets_from_dir(workspace_root)
  status_list = list(map(lambda x: _inspect_file(bazel_output_base, x), targets))
  sys.exit(-1 if any(status_list) else 0)
