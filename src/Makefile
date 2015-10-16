# Setup some aliases to these can be easily altered in the future.
include ../tos_conf
GCC:=g++
CFLAGS:=-Wall
LDFLAGS:=
INCLUDE:=$(TINYOS_ROOT_DIR)/tos/lib/tossim
APP_NAME:=tc
TOS_APP_DIR:=../tos_app
TOS_SIM_DIR:=../tos_app/simbuild/micaz
SRCS:=$(wildcard *.cpp) $(wildcard *.c)
SRCS:=$(SRCS)
OBJS:=$(subst .cpp,.o,$(SRCS)) \
	  $(TOS_SIM_DIR)/tossim.o \
	  $(TOS_SIM_DIR)/sim.o \
	  $(TOS_SIM_DIR)/c-support.o \
	  $(TOS_SIM_DIR)/sf.o \
	  $(TOS_SIM_DIR)/c-sf.o \
	  $(TOS_SIM_DIR)/throttle.o

# Link the object files together into the final executable.
all: ${APP_NAME} copy_files

${APP_NAME}: ${OBJS}
	$(GCC) -I $(INCLUDE) -o $(APP_NAME) $(OBJS) $(LDFLAGS)
	
copy_files:
	cp ${APP_NAME} ../

# Compile the individual code files into object files.

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(GCC) $(CFLAGS) -MM $^>>./.depend;	
	
.cpp.o:
	$(GCC) -I $(INCLUDE) $(CFLAGS) -c -o $@ $<

# Cleanup all of the auto-generated files

clean:
	cd $(TOS_APP_DIR); make clean
	cd $(TOS_APP_DIR); rm -f *.o *.dll
	rm -f ./$(APP_NAME) ../$(APP_NAME) *.o *~.

dist-clean: clean
	rm -f *~ .depend

include .depend
