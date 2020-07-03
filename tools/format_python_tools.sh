#!/bin/bash

set -e

VENV_DIR="pyformat"
SCRIPTPATH=$(realpath "$(dirname $0)")
. $SCRIPTPATH/shell_utils.sh
cd "$SCRIPTPATH"

source_venv "$VENV_DIR"
echo "Installing requirements..."
pip install -r requirements.txt

echo "Running Python format check..."
python format_python_tools.py $1

echo "Running Python3 flake8 check..."
cd ..
EXCLUDE="--exclude=benchmarks/tmp/*,*/venv/*,tools/format_python_tools.py,tools/gen_compilation_database.py,bazel-*"
flake8 . ${EXCLUDE} --count --select=E901,E999,F821,F822,F823 --show-source --statistics
# We raise the bar higher for benchmarks/ overall, but especially when it comes to docstrings.
# Check everything, except indentation and line length for now.
# We ignore unused imports and redefinitions of unused, as those seems to raise false flags in test definitions.
# We ignore formatting isssues E124/E125/E126 because they conflict with the automtic fix format feature.
flake8 . ${EXCLUDE} --ignore=E114,E111,E501,F401,F811,E124,E125,E126,D --count --show-source --statistics
flake8 . ${EXCLUDE} --docstring-convention pep257 --select=D --count --show-source --statistics
# Additional docstring checking based on Google's convention.
flake8 . ${EXCLUDE} --docstring-convention google --select=D --count --show-source --statistics

