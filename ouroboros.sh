#!/bin/bash

# ==============================================================================
# Ouroboros Build & Test Script
# ==============================================================================
#
# This script provides a streamlined workflow for building, testing, and
# running the TailRecursionElimination LLVM pass.
#
# Usage:
#   ./ouroboros.sh <command> [arguments]
#
# Commands:
#   config      : Configure the project with CMake for a debug build.
#   build       : Build the project using make.
#   rebuild     : Re-configure and build the project from scratch.
#   run <file>  : Run the pass on a specific LLVM IR file.
#   test        : Run the lit test suite.
#   clean       : Clean the build directory.
#   help        : Show this help message.
#
# ==============================================================================

set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration ---
LLVM_DIR=~/dev/llvm-project/llvm-build
BUILD_DIR=./build
PASS_LIB_NAME=libTailRecursionElimination.dylib
PASS_NAME=tailrecelim

# --- Helper Functions ---
print_usage() {
    echo "Usage: $0 <command> [arguments]"
    echo ""
    echo "Commands:"
    echo "  config      Configure the project with CMake for a debug build."
    echo "  build       Build the project using make."
    echo "  rebuild     Re-configure and build the project from scratch."
    echo "  run <file>  Run the pass on a specific LLVM IR file."
    echo "  test        Run the lit test suite."
    echo "  clean       Clean the build directory."
    echo "  help        Show this help message."
}

# --- Main Script Logic ---

# Show help message if no arguments are provided
if [ $# -eq 0 ]; then
    print_usage
    exit 0
fi

COMMAND=$1
shift # Remove the command from the arguments list

case $COMMAND in
    config)
        echo "--- Configuring project ---"
        mkdir -p ${BUILD_DIR}
        cd ${BUILD_DIR}
        cmake -DLLVM_INSTALL_DIR=${LLVM_DIR} -DLLVM_ENABLE_ASSERTIONS=ON ..
        ;;

    build)
        echo "--- Building project ---"
        cd ${BUILD_DIR}
        make -j$(nproc)
        ;;

    rebuild)
        echo "--- Rebuilding project from scratch ---"
        rm -rf ${BUILD_DIR}
        $0 config
        $0 build
        ;;

    run)
        if [ -z "$1" ]; then
            echo "Error: Missing file argument for 'run' command."
            print_usage
            exit 1
        fi
        INPUT_FILE=$1
        echo "--- Running pass on ${INPUT_FILE} ---"
        ${LLVM_DIR}/bin/opt \
            --load-pass-plugin ${BUILD_DIR}/lib/${PASS_LIB_NAME} \
            --passes="${PASS_NAME}" \
            -disable-output -stats -debug \
            ${INPUT_FILE}
        ;;

    test)
        echo "--- Running lit tests ---"
        cd ${BUILD_DIR}
        lit -a -v test
        ;;

    clean)
        echo "--- Cleaning build directory ---"
        rm -rf ${BUILD_DIR}
        ;;

    help|*)
        print_usage
        ;;
esac

echo "--- Done ---"
