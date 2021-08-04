#!/bin/bash

source /usr/local/oecore-x86_64/environment-setup-aarch64-poky-linux

if [ ${1} == "clean" ]; then
	rm -rf -rf baresip_build
	cd WebRTC_AEC
	make clean
	cd ../speex-1.2.0
	make clean
	cd ../speexdsp-1.2rc3
	make clean
	cd ../re
	make clean
	cd ../rem
	make clean
	cd ../baresip
	make clean
	cd ../	
	exit 0
fi

rm -rf baresip_build
cd WebRTC_AEC
./build.sh
cd ../speex-1.2.0
./build.sh
cd ../speexdsp-1.2rc3
./build.sh
cd ../re
./build.sh
cd ../rem
./build.sh
cd ../baresip
./build.sh
cd ../
cd baresip_build/usr/lib
cd ../../
mkdir -p home/root/.baresip
cp ../baresip_config/* home/root/.baresip

find . -name "*.a" -type f -delete
find . -name "*.la" -type f -delete

#create install package
cd ../
tar -czvf baresip_install.tar.gz baresip_build
