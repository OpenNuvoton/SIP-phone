#!/bin/bash

MACH=`uname -m`
PACKAGE_NAME=baresip_install.tar.gz
PACKAGE_FOLDER=baresip_build
TARGET_FOLDER=/usr
TARGET_CONFIG_FOLDER=/home/root


if [ "$MACH" == "aarch64" ]; then
	echo "OK, start install"
else
	echo "Target not match. Exit!"
	exit 1
fi

if [ ! -f "$PACKAGE_NAME" ]; then
	echo "Package not exist. Exit!"
	exit 2
fi

tar -xzvf $PACKAGE_NAME

cd $PACKAGE_FOLDER
cp -r home/root/* $TARGET_CONFIG_FOLDER
cp -rf usr/bin/* $TARGET_FOLDER/bin
cp -rf usr/lib/* $TARGET_FOLDER/lib
cd ../
rm -rf $PACKAGE_FOLDER

echo "Install done!"
exit 0
