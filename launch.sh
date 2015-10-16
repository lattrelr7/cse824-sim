#!/bin/sh
cygstart /bin/bash -c ./tc
sleep 1
cygstart /bin/bash -c ./tail_sim_dbg.sh
cygstart /bin/bash -c ./tail_tc_dbg.sh
