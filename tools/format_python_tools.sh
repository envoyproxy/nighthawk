#!/bin/bash

set -e

DIRECTORY=${PWD}

echo "Running Python format check..."
#bazel run //tools:format_python_tools -- --directory=${DIRECTORY} $1

echo "Running Python3 flake8 check..."
EXCLUDE="venv,format_python_tools.py,gen_compilation_database.py"

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
bazel run //tools:flake8 -- ${DIRECTORY} --exclude=${EXCLUDE} --ignore=E114,E111,E501,F401,F811,E124,E125,E126,W504,D --count --show-source --statistics
bazel run //tools:flake8 -- ${DIRECTORY} --exclude=${EXCLUDE} --docstring-convention pep257 --select=D --count --show-source --statistics
bazel run //tools:flake8 -- ${DIRECTORY} --exclude=${EXCLUDE} --docstring-convention google --select=D --count --show-source --statistics
