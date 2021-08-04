#!/bin/bash

./configure --host=aarch64-poky-linux --prefix=$PWD/../baresip_build/usr
make install
