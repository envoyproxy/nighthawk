#!/usr/bin/env python3

# Ensure that includes are referenced via external/envoy

import os, sys
from pathlib import Path

def get_inspection_targets_from_dir(dir):
    result = list(Path(dir).rglob("*.cc"))
    result.extend(Path(dir).rglob("*.h"))
    return result

def inspect_line(bazel_output_base, file_path, line):
    if line.startswith('#include "'):
        path = line[len("#include"):].strip(' "')
        found_in_nighthawk_sources = os.path.isfile("source/" + path) or os.path.isfile("include/" + path)
        
        if not found_in_nighthawk_sources:
            alternative_found = False
            potential_envoy_path = os.path.join(bazel_output_base, "external/envoy/", path)             
            alternative_found = os.path.isfile(potential_envoy_path)

            if not alternative_found:
                potential_envoy_path = os.path.join(bazel_output_base, "external/envoy/source", path)
                alternative_found = os.path.isfile(potential_envoy_path)          

            if alternative_found:
                sys.stderr.writelines("Bad include in file " + str(file_path) + ": " + path)
                sys.stderr.writelines(" (Possible fixed path: " + potential_envoy_path[len(bazel_output_base):] + ")\n")
                return False

    return True

def inspect_file(bazel_output_base, file_path):
    offending_lines = []
    with open(file_path) as f:
        lines = f.readlines()
        offending_lines.extend(l for l in lines if not inspect_line(bazel_output_base, file_path, l.strip()))
    return file_path, offending_lines, len(offending_lines) == 0 

workspace_root = sys.argv[2]
bazel_output_base = sys.argv[1]
targets = get_inspection_targets_from_dir(workspace_root)
status_list = list(map(lambda x:inspect_file(bazel_output_base, x), targets))
noncompliant_list = [path for path, offending_lines, status in status_list if not status]
sys.exit(0 if len(noncompliant_list) == 0 else -1)


