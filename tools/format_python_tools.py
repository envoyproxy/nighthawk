import argparse
import fnmatch
import os
import sys

from yapf.yapflib.yapf_api import FormatFile

# Directories in which we don't format files.
EXCLUDE_DIRECTORIES = ['generated', 'venv', ".cache"]

# Files that are excluded from formatting.
EXCLUDE_FILES = ['gen_compilation_database.py']


def collectFiles(directory):
  """Collect all Python files in the tools directory.

  Args:
    directory: A string, path to a directory where to look for files that should
      be checked for correct formatting.

  Returns: A collection of python files in the tools directory excluding
    any directories in the EXCLUDE_DIRECTORIES constant.
  """
  # TODO: Add ability to collect a specific file or files.
  matches = []
  path_parts = os.getcwd().split('/')
  for root, dirnames, filenames in os.walk(directory):
    dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRECTORIES]
    for filename in fnmatch.filter(filenames, '*.py'):
      if filename in EXCLUDE_FILES:
        continue
      matches.append(os.path.join(root, filename))
  return matches


def validateFormat(directory, fix=False):
  """Check the format of python files in the tools directory.

  Args:
    fix: a flag to indicate if fixes should be applied.
    directory: A string, path to a directory where to look for files that
      should be checked for correct formatting.
  """
  fixes_required = False
  failed_update_files = set()
  successful_update_files = set()
  for python_file in collectFiles(directory):
    reformatted_source, encoding, changed = FormatFile(python_file,
                                                       style_config='./tools/.style.yapf',
                                                       in_place=fix,
                                                       print_diff=not fix)
    if not fix:
      fixes_required = True if changed else fixes_required
      if reformatted_source:
        print(reformatted_source)
      continue
    file_list = failed_update_files if reformatted_source else successful_update_files
    file_list.add(python_file)
  if fix:
    displayFixResults(successful_update_files, failed_update_files)
    fixes_required = len(failed_update_files) > 0
  return not fixes_required


def displayFixResults(successful_files, failed_files):
  if successful_files:
    print('Successfully fixed {} files'.format(len(successful_files)))

  if failed_files:
    print('The following files failed to fix inline:')
    for failed_file in failed_files:
      print('  - {}'.format(failed_file))


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Tool to format python files.')
  parser.add_argument('action',
                      choices=['check', 'fix'],
                      default='check',
                      help='Fix invalid syntax in files.')
  parser.add_argument(
      '--directory',
      default='.',
      help='directory where to search for Python files that will be formatted. Default ".".')
  args = parser.parse_args()
  is_valid = validateFormat(args.directory, args.action == 'fix')
  sys.exit(0 if is_valid else 1)
