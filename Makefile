.PHONY: tos_app tc_sim all clean

all: tos_app db tc_sim
	
tos_app: 
	cd tos_app; make micaz sim-sf

db:
	 cat ./database_model/create824Tables.sql | sqlite3 824Sim.db
	
tc_sim:
	cd src/sim; make

clean:
	-rm 824Sim.db
	cd tos_app; make clean
	cd src/sim; make dist-clean
