Real Hardware Setup README

Setting up the virtual machine:
	My virtual machine setup:
	VMware Player 7.1.2
	Linux Mint 17.2 Cinnamon 64bit

	You can probably use any Debian based distro (Debian, Ubuntu, Mint), I happen to like Mint the best.

	Once the VM is set up:
	1. sudo nano /etc/apt/sources.list
	Add the line:  deb http://tinyos.stanford.edu/tinyos/dists/ubuntu lucid main
	2. sudo apt-get update
	3. sudo apt-get install tinyos-2.1.2
	4. Make a file called /opt/tinyos-2.1.2/tinyos.sh
	-----------------------------------------------------------------------
	#! /usr/bin/env bash

	# Here we setup the environment
	# variables needed by the tinyos
	# make system

	echo "Setting up for TinyOS 2.1.2 Repository Version"

	export TOSROOT=
	export TOSDIR=
	export MAKERULES=

	TOSROOT="/opt/tinyos-2.1.2"
	TOSDIR="$TOSROOT/tos"
	CLASSPATH=$CLASSPATH:$TOSROOT/support/sdk/java:.:$TOSROOT/support/sdk/java/tinyos.jar
	MAKERULES="$TOSROOT/support/make/Makerules"

	export TOSROOT
	export TOSDIR
	export CLASSPATH
	export MAKERULES
	-----------------------------------------------------------------------
	5. Go to /opt/tinyos-2.1.2/apps/Blink
	run the commands:
		 su
		 source /opt/tinyos-2.1.2/tinyos.sh
		 make iris

	6. Setup tinyos to work with our build environment
		type "sudo nano Makefile.include"
		add "include $(MAKERULES)" to the file and close it
		 
Configuring the VM
	- Install the 1.6 JDK
	- Build the java tools at tinyos-2.x/support/sdk/java to be able to use the serial tools provided.

To test to see if in Linux your serial is working, you can use the following
	- "java net.tinyos.tools.Listen -comm serial@/dev/ttyUSB1:iris"

to install the program
	-"make iris"
	-"make iris install,<node_id> mib520,/dev/ttyUSB0"
	- <node_id> = 0 is reserved for the basestation.
	
To get the Serial to work on cygwin
	- Find the COM port that windows assigned to the developement board (hardware manager)
	- in the terminal you're going to run type "chmod 666 /dev/ttyS#" where # is the value following COM minus 1
	- check to see if you can access it by typing "stty < /dev/ttyS#" and if you don't get "No such device or address" your good.
	