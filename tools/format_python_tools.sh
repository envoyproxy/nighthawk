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
EXCLUDE="--exclude=../benchmarks/tmp/*,*/venv/*"
flake8 . ${EXCLUDE} --count --select=E901,E999,F821,F822,F823 --show-source --statistics
# We raise the bar higher for benchmarks/ overall, but especially when it comes to docstrings.
# Check everything, except indentation and line length for now.
# Also, we ignore unused imports and redefinitions of unused, as those seems to raise false flags in test definitions.
flake8 ../benchmarks/ ${EXCLUDE} --docstring-convention pep257 --ignore=E114,E111,E501,F401,F811 --count --show-source --statistics
# Additional docstring checking based on Google's convention.
flake8 ../benchmarks/ ${EXCLUDE} --docstring-convention google --select=D --count --show-source --statistics
