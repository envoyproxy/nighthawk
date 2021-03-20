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
EXCLUDE="--exclude=benchmarks/tmp/*,.cache/*,*/venv/*,tools/format_python_tools.py,tools/gen_compilation_database.py,bazel-*"


# Because of conflict with the automatic fix format script, we ignore: 
# E111 Indentation is not a multiple of four
# E114 Indentation is not a multiple of four (comment)
# E501 Line too long (82 > 79 characters)
# E124 Closing bracket does not match visual indentation
# E125 Continuation line with same indent as next logical line
# E126 Continuation line over-indented for hanging indent
# W504 line break after binary operator

# We ignore false positives because of what look like pytest peculiarities 
# F401 Module imported but unused
# F811 Redefinition of unused name from line n
flake8 . ${EXCLUDE} --ignore=E114,E111,E501,F401,F811,E124,E125,E126,W504,D --count --show-source --statistics
# D = Doc comment related checks (We check both p257 AND google conventions). 
flake8 . ${EXCLUDE} --docstring-convention pep257 --select=D --count --show-source --statistics
flake8 . ${EXCLUDE} --docstring-convention google --select=D --count --show-source --statistics

