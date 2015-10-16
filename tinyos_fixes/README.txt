Replace the files:
lib/tossim/sf/sim/SerialActiveMessageC.nc
lib/tossim/sf/sim_serial_forwarder.c

This fixes length and offset issues when receiving serial messages from the simulation.

SerialActiveMessageC.nc sent a message starting 5 bytes before the serial data, so the first 5 bytes of data are not valid, and the last 5 bytes are never sent.  also had to fix receive, it was getting the header at an invalid location, resulting in the wrong data being sent.
sim_serial_forwarder.c added an extra byte to the message data for some reason in the dispatch function.