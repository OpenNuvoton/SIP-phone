Require:
	1. Install gdbus-codegen on host PC 
		#apt-get install libglib2.0-dev-bin

Build:
	1. Setup poky-linux-gcc environment variable
		#source /usr/local/oecore-x86_64/environment-setup-aarch64-poky-linux
	2. Run build all script
		#./buildall.sh

Run:
	1. Copy baresip_install.tar.gz and target_baresip_install.sh to target file system
	2. Run install script
		#./target_baresip_install.sh
	3. Launch dbus daemon
		#export $(dbus-launch)
	4. Execute baresip
		#baresip
