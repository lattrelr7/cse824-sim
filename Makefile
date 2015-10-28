.PHONY: tos_app tc all clean

all: tos_app tc
	
tos_app: 824Sim.db
	cd tos_app; make micaz sim-sf

824Sim.db:
	 cat ./database_model/create824Tables.sql | sqlite3 824Sim.db
	
tc:
	cd src; make

clean:
	cd tos_app; make clean
	cd src; make dist-clean
