#!/usr/bin/env python3

import logging
import os
import re
import sys
from subprocess import Popen, PIPE

if __name__ == '__main__':
  project_root = os.path.join(os.path.dirname(os.path.join(os.path.realpath(__file__))), "../")
  nighthawk_client_path = os.path.join(project_root)
  readme_md_path = os.path.join(project_root, "README.md")
  print("project root: %s" % os.path.realpath(project_root))
  os.chdir(os.path.realpath(project_root))
  print(os.getcwd())
  process = Popen(["bazel-bin/nighthawk_client", "--help"], stdout=PIPE)
  (output, err) = process.communicate()
  exit_code = process.wait()
  if exit_code != 0:
    sys.stderr.writelines(["Failure running the nighthawk client's help command"])
    sys.exit(exit_code)

  with open(readme_md_path, "r") as f:
    replaced = re.sub("\nUSAGE\:[^b]*.*bazel-bin\/nighthawk_client[^```]*", output.decode(), f.read())

  with open(readme_md_path, "w") as f:
      f.write("%s" % replaced)

  print("done")
