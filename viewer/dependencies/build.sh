#!/usr/bin/env bash

DEPENDENCIES="Eigen glfw vrpn"
for DEP in $DEPENDENCIES; do
	pushd buildfiles/$DEP > /dev/null
	echo "====================================================================="
	echo "Downloading, building and installing $DEP..."
	./fetch.sh
	if [[ -f "build.sh" ]]; then
		./build.sh
	fi
	popd > /dev/null
done