#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# Fix Mono and engine dependencies if needed
START_DIR=`pwd`
cd "$1"

export HOST_ARCH=x86_64-unknown-linux-gnu

bash FixMonoFiles.sh
bash FixDependencyFiles.sh
IS_MONO_INSTALLED=1
IS_MS_BUILD_AVAILABLE=1
MONO_VERSION_PATH=$(command -v mono) || true

cd "$START_DIR"
