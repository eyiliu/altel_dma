#! /usr/bin/env bash

BASH_SOURCE_REAL_PATH=$(readlink -m ${BASH_SOURCE})
BASH_SOURCE_DIR=$(dirname ${BASH_SOURCE_REAL_PATH})
BASH_SOURCE_BASE_NAME=$(basename $0)

echo "${BASH_SOURCE_DIR}/module/alteldma.ko"
insmod ${BASH_SOURCE_DIR}/module/alteldma.ko
