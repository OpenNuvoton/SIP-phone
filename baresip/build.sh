#!/bin/bash
#Must install glib2.0-dev package on host. #apt install libglib2.0-dev
export OS=linux
make SYSROOT="$OECORE_TARGET_SYSROOT/usr"  V="1" DESTDIR=../baresip_build install
unset OS
