#!/usr/bin/env python3
"""Tool to automatically update README.md files that contain --help output of c++ TCLAP binaries.

It runs the specified binary with the --help flag and replaces all content
between our usage markers in the specified readme.

The readme has to have the following edit markers:

Unrelated text.
<!-- BEGIN USAGE -->

Text to be replaced.

<!-- END USAGE -->
"""

import logging
import argparse
import os
import pathlib
import re
import sys
import subprocess

# The marker expected at the beginning of the text section that should be
# replaced.
_BEGIN_MARKER = "<!-- BEGIN USAGE -->"

# The marker expected at the end of the text section that should be
# replaced.
_END_MARKER = "<!-- END USAGE -->"

# Template used when creating a regex pattern that replaces text between the
# markers, if it is for the specified binary.
# Matches a string like:
# <!-- BEGIN USAGE -->
# ```
#
# USAGE:
#
# bazel-bin/nighthawk_test_server ...
#
# ```
# <!-- END USAGE -->
#
# Note ".*?" performs a non-greedy match, since some of our readme files contain
# multiple USAGE sections.
_REPLACEMENT_PATTERN_TEMPLATE = r"{begin_marker}[`\s]+USAGE:\s+{binary}.*?{end_marker}"

# Template used to construct the replacement.
_REPLACEMENT_TEMPLATE = "{begin_marker}\n```\n{cli_help_text}\n```\n{end_marker}"

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Tool to update README.md CLI documentation.")
  parser.add_argument(
      "--binary",
      required=True,
      help="Relative path to the target binary, for example: \"bazel-bin/nighthawk_client\".")
  parser.add_argument(
      "--readme",
      required=True,
      help="Relative path to the target documentation file, for example: \"README.md\"")
  parser.add_argument("--mode",
                      default="check",
                      required=True,
                      choices={"check", "fix"},
                      help="Either \"check\" or \"fix\"")

  args = parser.parse_args()
  logging.getLogger().setLevel(logging.INFO)
  project_root = os.path.join(os.path.dirname(os.path.join(os.path.realpath(__file__))), "../")
  # Change directory to avoid TCLAP outputting a full path specification to the binary
  # in its help output.
  os.chdir(os.path.realpath(project_root))
  readme_md_path = os.path.join(project_root, args.readme)
  process = subprocess.run([args.binary, "--help"], stdout=subprocess.PIPE, check=True)

  # Avoid trailing spaces, as they introduce markdown linting errors.
  cli_help_lines = [s.strip() for s in process.stdout.decode().splitlines()]
  cli_help_text = str.join("\n", cli_help_lines)

  target_path = pathlib.Path(readme_md_path)
  with target_path.open("r", encoding="utf-8") as f:
    original_contents = target_path.read_text(encoding="utf-8")
    replacement_pattern = _REPLACEMENT_PATTERN_TEMPLATE.format(begin_marker=_BEGIN_MARKER, binary=args.binary, end_marker=_END_MARKER)
    match_pattern = f".*{replacement_pattern}.*"
    if not re.match(match_pattern, original_contents, flags=re.DOTALL):
        logging.error("The original content in /%s doesn't match our replacement pattern '%s'. "
                      "If the file has the expected markers, this is likely a bug in "
                      "update_cli_readme_documentation.py.",
                      args.readme, match_pattern)
        sys.exit(-1)

    replacement = _REPLACEMENT_TEMPLATE.format(begin_marker=_BEGIN_MARKER, cli_help_text=cli_help_text,
                                               end_marker=_END_MARKER)
    replaced = re.sub(replacement_pattern, replacement, original_contents, flags=re.DOTALL)
    # Avoid check_format flagging "over-enthousiastic" whitespace
    replaced = replaced.replace("...  [", "... [")

  if replaced != original_contents:
    if args.mode == "check":
      logging.info("CLI documentation in /%s needs to be updated for %s" %
                   (args.readme, args.binary))
      sys.exit(-1)
    elif args.mode == "fix":
      with target_path.open("w", encoding="utf-8") as f:
        logging.error("CLI documentation in /%s needs to be updated for %s" %
                      (args.readme, args.binary))
        f.write("%s" % replaced)

  logging.info("Done")
  sys.exit(0)
