.PHONY: tos_app tc all clean

all: tos_app tc
	
tos_app:
	cd tos_app; make micaz sim-sf
	
tc:
	cd src; make

clean:
	cd tos_app; make clean
	cd src; make dist-clean
