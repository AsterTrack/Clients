#!/usr/bin/env bash

# FETCH_URL=https://github.com/vrpn/vrpn/archive/refs/heads/master.zip
# Frozen 7.35
#FETCH_URL=https://github.com/vrpn/vrpn/archive/refs/tags/version_07.35.zip
#FETCH_VERSION=7.35
# Frozen 7.36 with regression fixes and support for python 3.13
FETCH_URL=https://github.com/vrpn/vrpn/archive/844c5873d71b70a51825f2a8b9928bfc9a84c717.zip
FETCH_VERSION=7.36-master
FETCH_NAME=vrpn
FETCH_ARCHIVE=source.zip

if [[ -f "source/srcversion" ]]; then
	if [[ $(cat source/srcversion) != $FETCH_VERSION ]]; then
		rm "source/srcversion"
	fi
fi

if [[ ! -f "source/srcversion" ]]; then
	echo Downloading $FETCH_NAME $FETCH_VERSION source
	wget -O $FETCH_ARCHIVE $FETCH_URL -q --show-progress
	if [[ $? -ne 0 ]]; then
		echo Failed to download source!
		rm $FETCH_ARCHIVE
		exit 1
	fi

	if [[ -d "source" ]]; then
		rm -r source
	fi

	echo Unpacking $FETCH_NAME $FETCH_VERSION source
	unzip -q $FETCH_ARCHIVE
	mv $FETCH_NAME-* source

	rm $FETCH_ARCHIVE
	echo $FETCH_VERSION > source/srcversion
	echo Done downloading $FETCH_NAME $FETCH_VERSION!
fi