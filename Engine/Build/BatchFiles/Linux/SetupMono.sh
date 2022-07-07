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

# Setup bundled Mono if cannot use installed one
if [ $IS_MONO_INSTALLED -eq 0 ]; then
	echo Setting up Mono
	CUR_DIR=$PWD
	export UE_MONO_DIR=$CUR_DIR/../../../Binaries/ThirdParty/Mono/Linux
	export PATH=$UE_MONO_DIR/bin:$PATH
	export MONO_PATH=$UE_MONO_DIR/lib/mono/4.5:$MONO_PATH
	export MONO_CFG_DIR=$UE_MONO_DIR/etc
	export LD_LIBRARY_PATH=$UE_MONO_DIR/$HOST_ARCH/lib:$LD_LIBRARY_PATH
else
	export IS_MONO_INSTALLED=$IS_MONO_INSTALLED
	export IS_MS_BUILD_AVAILABLE=$IS_MS_BUILD_AVAILABLE
fi

cd "$START_DIR"
