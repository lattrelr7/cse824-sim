.PHONY: tos_app tc all clean

all: tos_app db tc
	
tos_app: 
	cd tos_app; make micaz sim-sf

db:
	 cat ./database_model/create824Tables.sql | sqlite3 824Sim.db
	
tc:
	cd src; make

clean:
	rm 824Sim.db
	cd tos_app; make clean
	cd src; make dist-clean
