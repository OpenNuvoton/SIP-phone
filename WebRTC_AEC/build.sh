#!/bin/bash

./configure --host=aarch64-poky-linux --prefix=`pwd`/../baresip_build/usr
make install
