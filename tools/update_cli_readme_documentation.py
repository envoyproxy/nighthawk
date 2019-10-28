#!/usr/bin/env python3

import logging
import argparse
import os
import re
import sys
from subprocess import Popen, PIPE

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Tool to update README.md CLI documentation.')
  parser.add_argument(
      '--binary',
      help='Relative path to the target binary, for example: "bazel-bin/nighthawk_client".')
  parser.add_argument(
      '--readme', help='Relative path to the target documentation file, for example: "README.md"')
  args = parser.parse_args()

  project_root = os.path.join(os.path.dirname(os.path.join(os.path.realpath(__file__))), "../")
  # Change directory to avoid TCLAP outputting a full path specification to the binary
  # in its help output.
  os.chdir(os.path.realpath(project_root))
  readme_md_path = os.path.join(project_root, args.readme)
  process = Popen([args.binary, "--help"], stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  if exit_code != 0:
    sys.stderr.writelines(["Failure running --help on with target command"])
    sys.exit(exit_code)

  # Avoid trailing spaces, as they introduce markdown linting errors.
  cli_help = [s.strip() for s in output.decode().splitlines()]

  with open(readme_md_path, "r") as f:
    replaced = re.sub("\nUSAGE\:[^.]*.*%s[^```]*" % args.binary, str.join("\n", cli_help), f.read())

  with open(readme_md_path, "w") as f:
    f.write("%s" % replaced)

  print("done")
