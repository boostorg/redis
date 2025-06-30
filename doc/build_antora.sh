#!/bin/bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR"

# Required by our CMake.
# Prevents Antora from cloning Boost again
export BOOST_SRC_DIR=$(realpath $SCRIPT_DIR/../../..)

npm ci
npx antora --log-format=pretty redis-playbook.yml
