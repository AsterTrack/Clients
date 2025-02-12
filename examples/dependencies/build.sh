#!/usr/bin/env bash

# Select release or debug mode
MODE="$(echo $1 | tr '[:upper:]' '[:lower:]')"

DEPENDENCIES="vrpn"
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