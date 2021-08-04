#!/bin/bash

export OS=linux
make SYSROOT="$OECORE_TARGET_SYSROOT/usr" DESTDIR=../baresip_build install
unset OS
